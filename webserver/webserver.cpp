//
// Created by 20771 on 2023/12/22.
//
#include "webserver.h"

using namespace std;

int WebServer::m_pipefd[2]{};

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] http_users;
    delete m_thread_pool;
}

WebServer::WebServer(int port,
                     string log_save_path,
                     string user,
                     string pass_word,
                     string data_base_name,
                     int asyn_log_write,
                     int opt_linger,
                     int trig_mode,
                     int sql_num,
                     int thread_num,
                     int close_log,
                     int actor_model) :

                     m_port(port),
                     m_log_save_path(std::move(log_save_path)),
                     m_user(std::move(user)),
                     m_pass_word(std::move(pass_word)),
                     m_data_base_name(std::move(data_base_name)),
                     m_asyn_log_write(asyn_log_write),
                     m_OPT_LINGER(opt_linger),
                     m_TRIGMode(trig_mode),
                     m_max_sql_num(sql_num),
                     m_thread_num(thread_num),
                     m_close_log(close_log),
                     m_actor_model(actor_model) {

    //http_conn类对象
    http_users = new HttpConn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";

    strcpy(m_root, server_path);
    strcat(m_root, root);

    //设置监听套接字和连接套接字模式
    set_trig_mode();

    //初始化日志模块
    init_log();

    //初始化数据库连接池
    init_sql_pool();

    //初始化线程池
    init_thread_pool();
}

void WebServer::set_trig_mode() {
    //LT + LT
    if (0 == m_TRIGMode) {
        m_LISTEN_TRIGMode = 0;
        m_CONN_TRIGMode = 0;
    }
    //LT + ET
    else if (1 == m_TRIGMode) {
        m_LISTEN_TRIGMode = 0;
        m_CONN_TRIGMode = 1;
    }
    //ET + LT
    else if (2 == m_TRIGMode) {
        m_LISTEN_TRIGMode = 1;
        m_CONN_TRIGMode = 0;
    }
    //ET + ET
    else if (3 == m_TRIGMode) {
        m_LISTEN_TRIGMode = 1;
        m_CONN_TRIGMode = 1;
    }
}

void WebServer::init_sql_pool() {
    m_sql_conn_pool = SQLConnPool::get_instance();
    m_sql_conn_pool->init("localhost", m_user, m_pass_word, m_data_base_name,
                          3306, m_max_sql_num, m_close_log);

    // 将数据库中的用户名和密码加载至 m_users_info 中
   get_mysql_result(m_sql_conn_pool);
}

void WebServer::get_mysql_result(SQLConnPool* sql_conn_pool) {
    //先从连接池中取一个连接
    MYSQL *mysql = nullptr;
    ConnectionRAII mysql_conn(&mysql, m_sql_conn_pool);

    //在user表中检索username，passwd数据，浏览器端输入
    if(mysql_query(mysql, "SELECT username, passwd FROM user"))
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        m_users_info[temp1] = temp2;
    }
}

void WebServer::init_log() const {
    if (0 == m_close_log) {
        if (1 == m_asyn_log_write)
            Log::get_instance()->init(m_log_save_path.c_str(), 2000, 800000, 800);
        else
            Log::get_instance()->init(m_log_save_path.c_str(), 2000, 800000, 0);
    }
}

void WebServer::init_thread_pool() {
    try {
        m_thread_pool = new ThreadPool<HttpConn>(m_actor_model, m_thread_num);
    }
    catch (...) {
        LOG_ERROR("Init Thread Pool Failed")
        exit(1);
    }
}

void WebServer::event_listen() {
    //网络编程基础步骤
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 优雅关闭连接
    if(m_OPT_LINGER == 0) {
        struct linger opt = {0, 1}; // 不使用
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &opt, sizeof(opt));
    }
    else {
        struct linger opt = {1, 1}; // 使用
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &opt, sizeof(opt));
    }

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    struct sockaddr_in address{};
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int ret = 0;
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);

    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    //epoll创建内核事件表
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    HttpConn::m_epollfd =m_epollfd;
    addfd(m_epollfd, m_listenfd, false, m_LISTEN_TRIGMode);


    //设置信号通知事件
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    setnonblocking(m_pipefd[1]);
    addfd(m_epollfd, m_pipefd[0], false, 0);
    addsig(SIGPIPE, SIG_IGN);
    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);

    alarm(TIMESLOT);
}

bool WebServer::deal_client_connect() {
    struct sockaddr_in client_address{};
    socklen_t client_addrlength = sizeof(client_address);

    if(m_LISTEN_TRIGMode == 0) { // 条件触发，只读取一次
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0) {
            LOG_ERROR("%s:errno is:%d", "accept error", errno)
            return false;
        }
        if (HttpConn::m_user_count >= MAX_FD) {
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        init_client_conn(connfd, client_address);

        int clnt_port = client_address.sin_port;
        char* clnt_ip = inet_ntoa(client_address.sin_addr);
        LOG_INFO("New Client Connect (socket: %d)(%s: %d)", connfd, clnt_ip, clnt_port)
    }
    else { // 边缘触发，循环读取，直至无数据可读
        while (true) {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0) {
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                else
                    LOG_ERROR("%s:errno is:%d", "accept error", errno)
                    return false;
            }
            if (HttpConn::m_user_count >= MAX_FD) {
                LOG_ERROR("%s", "Internal server busy");
                return false;
            }
            init_client_conn(connfd, client_address);

            int clnt_port = client_address.sin_port;
            char* clnt_ip = inet_ntoa(client_address.sin_addr);
            LOG_INFO("New Client Connect (socket: %d)(%s: %d)", connfd, clnt_ip, clnt_port)
        }
    }

    return true;
}

bool WebServer::deal_with_signal(bool& timeout, bool& stop_server) {
    char signals[1024];
    size_t ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if(ret == -1 || ret == 0)
        return false;
    else {
        for(int i = 0; i < ret; ++i) {
            switch (signals[i]) {
                case SIGALRM: {
                    timeout = true;
                    break;
                }
                case SIGTERM: {
                    stop_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

void WebServer::deal_with_read(int connfd) {
    // Reactor模式，从线程负责I/O处理
    if(m_actor_model == 1) {
        adjust_timer(connfd);
        m_thread_pool->append(&http_users[connfd], 0);
        while (true) {
            if(http_users[connfd].improv == 1) {
                if(http_users[connfd].timer_flag == 1) {
                    close_client_conn(connfd);
                    http_users[connfd].timer_flag = 0;
                }
                http_users[connfd].improv = 0;
                break;
            }
        }
    }
    // 模拟Proactor模式，主线程负责I/O处理
    else {
        if(http_users[connfd].read()) {
            LOG_INFO("deal with read client(%s, %d)",
                     inet_ntoa(http_users[connfd].get_address().sin_addr),
                     http_users[connfd].get_address().sin_port)
            m_thread_pool->append(&http_users[connfd]);
            adjust_timer(connfd);
        }
        else {
            close_client_conn(connfd);
        }
    }
}

void WebServer::deal_with_write(int connfd) {
    // Reactor模式，从线程负责I/O处理
    if(m_actor_model == 1) {
        adjust_timer(connfd);
        m_thread_pool->append(&http_users[connfd], 1);
        while (true) {
            if(http_users[connfd].improv == 1) {
                if(http_users[connfd].timer_flag == 1) {
                    close_client_conn(connfd);
                    http_users[connfd].timer_flag = 0;
                }
                http_users[connfd].improv = 0;
                break;
            }
        }
    }
    // 模拟Proactor模式，主线程负责I/O处理
    else {
        if(http_users[connfd].write()) {
            LOG_INFO("send data to the client(%s, %d)",
                     inet_ntoa(http_users[connfd].get_address().sin_addr),
                     http_users[connfd].get_address().sin_port)
            adjust_timer(connfd);
        }
        else
            close_client_conn(connfd);
    }
}

void WebServer::init_client_conn(int connfd, struct sockaddr_in client_address) {
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    time_t cur = time(nullptr);
    time_t expire_t = cur + 3 * TIMESLOT;
    auto* timer = new TimerNode(expire_t, &http_users[connfd], timer_call_back);
    m_timer_list.add_timer(timer);

    //初始化client_data数据
    http_users[connfd].init(connfd, client_address, timer, m_root,
                            &m_users_info, m_sql_conn_pool,
                            m_CONN_TRIGMode, m_close_log);
}

void WebServer::close_client_conn(int sockfd) {
    TimerNode* timer = http_users[sockfd].get_timer();
    if(timer)
        m_timer_list.del_timer(timer);

    http_users[sockfd].close_conn();

}

void WebServer::adjust_timer(int sockfd) {
    TimerNode* timer = http_users[sockfd].get_timer();
    if(timer) {
        time_t cur_time = time(nullptr);
        timer->expire = cur_time + 3 * TIMESLOT;
        m_timer_list.adjust_timer(timer);

        LOG_INFO("(socket %d) adjust timer once", sockfd);
    }
}

void WebServer::event_loop() {
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server) {
        int num_events = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (num_events < 0 && errno != EINTR) {
            LOG_ERROR("%s", "epoll failure")
            stop_server = true;
            continue;
        }

        for(int i = 0; i < num_events; ++i) {
            int sockfd = events[i].data.fd;

            //处理新客户连接
            if(sockfd == m_listenfd) {
                if(!deal_client_connect())
                    continue;
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //服务器端关闭连接，移除对应的定时器
                close_client_conn(sockfd);
            }
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {
                if(!deal_with_signal(timeout, stop_server))
                    LOG_ERROR("%s", "deal_with_signal failure")
            }
            else if(events[i].events & EPOLLIN) {
                deal_with_read(sockfd);
            }
            else if(events[i].events & EPOLLOUT) {
                deal_with_write(sockfd);
            }
        }
        if(timeout) {
            timer_handler(m_timer_list, TIMESLOT);
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}
