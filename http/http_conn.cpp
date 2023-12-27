//
// Created by 20771 on 2023/12/20.
//

#include "http_conn.h"

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

int HttpConn::m_user_count = 0;
int HttpConn::m_epollfd = -1;
Locker HttpConn::m_lock;

// 关闭连接，关闭一个连接，客户总量减一
void HttpConn::close_conn(bool real_close) {
    if (real_close && (m_sockfd != -1))
    {
        int clnt_port = m_address.sin_port;
        char* clnt_ip = inet_ntoa(m_address.sin_addr);
        LOG_INFO("close %d(%s: %d)", m_sockfd, clnt_ip, clnt_port);

        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

//取消文件映射
void HttpConn::unmap() {
    if(m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = nullptr;
    }
}

//初始化连接,外部调用初始化套接字地址
void HttpConn::init(int sockfd, const sockaddr_in &addr, TimerNode* timer,
                    char *root, infoMap* users_info, SQLConnPool* sql_conn_pool,
                    int TRIGMode, int close_log) {
    m_sockfd = sockfd;
    m_address = addr;
    m_timer = timer;
    m_TRIGMode = TRIGMode;
    doc_root = root;
    m_close_log = close_log;
    m_users_info = users_info;
    m_sql_conn_pool = sql_conn_pool;

    addfd(m_epollfd, m_sockfd, true, m_TRIGMode);
    ++m_user_count;

    init();
}

//初始化新接受的连接
void HttpConn::init() {
    mysql = nullptr;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    m_start_line = 0;
    m_read_idx = 0;
    m_checked_idx = 0;
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    m_write_idx = 0;

    m_check_state = CHECK_STATE_REQUESTLINE;
    m_method = GET;
    m_url = nullptr;
    m_version = nullptr;
    m_host = nullptr;
    m_content_length = 0;
    m_linger = false;

    memset(m_real_file, '\0', FILENAME_LEN);
    m_file_address = nullptr;
    bytes_to_send = 0;
    bytes_have_send = 0;

    m_content_string = nullptr;
}

//分析http请求报文（请求行，头部字段，数据内容）
//返回http处理结果码
HttpConn::HTTP_CODE HttpConn::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = nullptr;

    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
            (line_status = parse_line()) == LINE_OK) {
        text = m_read_buf + m_start_line;
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);

        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE: // 请求行
            {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:     // 头部字段
            {
                ret = parse_headers(text);
                if(ret == GET_REQUEST)
                    return do_request();
                break;
            }
            case CHECK_STATE_CONTENT:   // 数据内容
            {
                ret = parse_content(text);
                if(ret == GET_REQUEST)
                    return do_request();
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }

    if(line_status == LINE_OPEN)
        return NO_REQUEST;
    else
        return BAD_REQUEST;
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
HttpConn::LINE_STATUS HttpConn::parse_line() {
    for(; m_checked_idx < m_read_idx; ++m_checked_idx){
        char temp = m_read_buf[m_checked_idx];
        if(temp == '\r') {
            if((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if(m_read_buf[m_checked_idx + 1] == '\n') {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n') {
            if(m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//解析http请求行，获得请求方法，目标url及http版本号
HttpConn::HTTP_CODE HttpConn::parse_request_line(char *text) {
    // 获取请求方法
    m_url = strpbrk(text, " \t");
    if(!m_url)
        return BAD_REQUEST;
    *m_url++ = '\0';
    char* method = text;
    if(strcasecmp(method, "GET") == 0) {
        m_method = GET;
    }
    else if(strcasecmp(method, "POST") == 0) {
        m_method = POST;
    }
    else
        return BAD_REQUEST;


    // 获取目标url
    m_url += strspn(m_url, " \t"); //跳过连续的“ \t”
    m_version = strpbrk(m_url, " \t");
    if(!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    if(strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if(strncasecmp(m_url, "https://", 8) == 0) {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if(!m_url || m_url[0] != '/')
        return BAD_REQUEST;

    // 获取http版本号
    m_version += strspn(m_version, " \t");
    if(strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    // 处理完请求行，将主状态机状态转移为 CHECK_STATE_HEADER
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析http请求的一个头部信息，本项目只解析 Connection, Content-length 和 Host 三个头部字段
HttpConn::HTTP_CODE HttpConn::parse_headers(char *text) {
    // 读取到空行，头部解析完成，判断是否含有消息体
    if(text[0] == '\0') {
        if(m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    if(strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    }
    else if(strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
//    else
//        LOG_INFO("oop! Unknown header: %s", text);

    return NO_REQUEST;
}

//判断http请求是否被完整读入
HttpConn::HTTP_CODE HttpConn::parse_content(char *text) {
    if(m_read_idx >= (m_checked_idx + m_content_length)) {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的 用户名 和 密码
        m_content_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 响应请求
HttpConn::HTTP_CODE HttpConn::do_request() {
    strcpy(m_real_file, doc_root);
    size_t doc_root_len = strlen(doc_root);
    const char* p = strrchr(m_url, '/');
    char flag = *(p + 1);

    if(m_method == POST && (flag == '2' || flag == '3')) {
        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_content_string[i] != '&'; ++i)
            name[i - 5] = m_content_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_content_string[i] != '\0'; ++i, ++j)
            password[j] = m_content_string[i];
        password[j] = '\0';

        std::string m_url_real;
        if(flag == '3') {
            // 建立mysql连接
            ConnectionRAII mysql_conn(&mysql, m_sql_conn_pool);

            //如果是注册，先检测数据库中是否有重名的
            std::string sql_insert = "INSERT INTO user(username, passwd) VALUES";
            std::string insert_name = "'" + std::string(name) + "'";
            std::string insert_password = "'" + std::string(password) + "'";
            sql_insert = sql_insert + "(" + insert_name + ", " + insert_password + ")";

            //没有重名的，进行增加数据
            if(m_users_info->find(name) == m_users_info->end()) {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert.c_str());
                m_users_info->insert({name, password});
                m_lock.unlock();

                if (!res)
                    m_url_real = "/log.html";
                else
                    m_url_real = "/registerError.html";
            }
            else
                m_url_real = "/registerError.html";
        }
        else {
            //如果是登录，直接判断
            if(m_users_info->find(name) != m_users_info->end() && (*m_users_info)[name] == password)
                m_url_real = "/welcome.html";
            else
                m_url_real = "/logError.html";
        }

        strncpy(m_real_file + doc_root_len, m_url_real.c_str(), FILENAME_LEN - doc_root_len - 1);
    }

    if(m_method == GET) {
        std::string m_url_real(m_url);

        if(flag == '0')
            m_url_real = "/register.html";
        else if(flag == '1')
            m_url_real = "/log.html";
        else if(flag == '5')
            m_url_real = "/picture.html";
        else if(flag == '6')
            m_url_real = "/video.html";
        else if(flag == '7')
            m_url_real = "/fans.html";
        else {
            if(m_url_real == "/")
                m_url_real += "judge.html";
        }

        strncpy(m_real_file + doc_root_len, m_url_real.c_str(), FILENAME_LEN - doc_root_len - 1);
    }

    if(stat(m_real_file, &m_file_stat) < 0) // 获取文件状态
        return NO_RESOURCE;

    if(!(m_file_stat.st_mode & S_IROTH)) // 权限不足
        return FORBIDDEN_REQUEST;

    if(S_ISDIR(m_file_stat.st_mode)) // 为目录，访问错误
        return BAD_REQUEST;

    // 将文件映射至内存
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(nullptr, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    return FILE_REQUEST;
}

//处理http回应报文（状态行，头部字段，数据内容）
//将回m_iv[0]初始化为头部，将m_iv[1]初始化为消息体，返回写状态结果成功为true失败为false
bool HttpConn::process_write(HTTP_CODE ret) {
    switch (ret) {
        case INTERNAL_ERROR: {
            if( !(add_status_line(500, error_500_title) &&
                add_headers(strlen(error_500_title)) &&
                add_content(error_500_form)) )
                return false;
            break;
        }
        case BAD_REQUEST: {
            if( !(add_status_line(400, error_400_title) &&
                  add_headers(strlen(error_400_title)) &&
                  add_content(error_400_form)) )
                return false;
            break;
        }
        case NO_RESOURCE: {
            if( !(add_status_line(404, error_404_title) &&
                  add_headers(strlen(error_404_title)) &&
                  add_content(error_404_form)) )
                return false;
            break;
        }
        case FORBIDDEN_REQUEST: {
            if( !(add_status_line(403, error_403_title) &&
                  add_headers(strlen(error_403_title)) &&
                  add_content(error_403_form)) )
                return false;
            break;
        }
        case FILE_REQUEST: {
            if(!add_status_line(200, ok_200_title)) return false;

            if(m_file_stat.st_size != 0) {
                if(!add_headers(m_file_stat.st_size)) return false;
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else {
                const char *ok_string = "<html><body></body></html>";
                if ( !(add_headers(strlen(ok_string)) &&
                        add_content(ok_string)) )
                    return false;
            }
        }
        default:
            return false;
    }

    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

bool HttpConn::add_status_line(int status, const char* title){
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}

bool HttpConn::add_headers(size_t content_len){
    return add_response( "Content-Length: %d\r\n", content_len ) &&
           add_response("Connection: %s\r\n", m_linger ? "keep-alive" : "close" ) &&
//           add_response("Content-Type:%s\r\n", "text/html") &&
           add_response( "%s", "\r\n" );
}

bool HttpConn::add_content(const char* content){
    return add_response( "%s", content );
}

bool HttpConn::add_response( const char* format, ... ){
    if( m_write_idx >= WRITE_BUFFER_SIZE )
        return false;

    va_list arg_list;
    va_start( arg_list, format );

    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - m_write_idx - 1, format, arg_list );
    if(len >= (WRITE_BUFFER_SIZE - m_write_idx - 1))
        return false;
    m_write_idx += len;

    va_end( arg_list );
    return true;
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool HttpConn::read() {
    if(m_read_idx >= READ_BUFFER_SIZE)
        return false;

    if(m_TRIGMode == 0) { // LT读数据
        long read_len = recv(m_sockfd, m_read_buf, READ_BUFFER_SIZE - m_read_idx, 0);
        if(read_len <= 0)
            return false;

        m_read_idx += read_len;
    }
    else { // ET读数据,非阻塞读
        while(true) {
            long read_len = recv(m_sockfd, m_read_buf, READ_BUFFER_SIZE - m_read_idx, 0);
            if(read_len == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if(read_len == 0)
                return false;

            m_read_idx += read_len;
        }
    }
    return true;
}

//循环写入响应数据，直到数据全部写入
//返回ture继续保持连接，否则断开连接
bool HttpConn::write() {
    if(bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while(true) {
        long temp = writev(m_sockfd, m_iv, m_iv_count);
        if(temp < 0) {
            if(errno == EAGAIN) {
                // 写缓冲区满，保持连接，将套接字事件写事件重置，监听下一次可写事件
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        if (bytes_have_send == bytes_to_send) { // 全部发送完成
            unmap();
            // 发送完成将套接字重新注册为EPOLLIN事件
            modfd( m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            // 判断是否为 keep-alive，是的话保持连接，否则断开连接
            if( m_linger ) {
                init();
                return true;
            }
            else
                return false;
        }
        // 没全部发送，更新m_iv中相应的值，等待下一次发送
        else if(bytes_have_send <= m_write_idx) {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }
        else {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send - bytes_have_send;
        }
    }
}

//http请求报文和回应报文逻辑处理
void HttpConn::process() {
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST) {
        // 数据未全部接受，重置EPOLLIN事件
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }

    bool write_ret = process_write(read_ret);
    if(!write_ret)
        close_conn(); // 写处理失败，关闭连接
    // 写处理成功，注册EPOLLOUT事件
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}
