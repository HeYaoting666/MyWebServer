//
// Created by 20771 on 2023/12/20.
//

#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <unordered_map>
#include <mysql/mysql.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/uio.h>

#include "../pool/sql_conn_pool.h"
#include "../lock/locker.h"
#include "../log/log.h"
#include "../commend/utils.h"

class TimerNode;

class HttpConn {
public:
    typedef std::unordered_map<std::string, std::string> infoMap;

    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    // 请求方法
    enum METHOD
    {
        GET,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    // 从状态机状态
    enum LINE_STATUS
    {
        LINE_OK,    // 行读取成功，读取一整行
        LINE_BAD,   // 行读取失败，行出错
        LINE_OPEN   // 行读取不完整
    };
    // 主状态机状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE, // 正在分析请求行
        CHECK_STATE_HEADER,      // 正在分析头部字段
        CHECK_STATE_CONTENT      // 正在分析数据内容
    };
    // 服务器处理http请求结果
    enum HTTP_CODE
    {
        NO_REQUEST,         // 请求不完整
        GET_REQUEST,        // 获取到完整请求
        BAD_REQUEST,        // 客户请求有语法错误
        NO_RESOURCE,        // 没有对应资源
        FORBIDDEN_REQUEST,  // 客户对资源无访问权限
        FILE_REQUEST,       // 获取文件请求
        INTERNAL_ERROR,     // 服务器内部错误
        CLOSED_CONNECTION   // 客户端关闭连接
    };

public:
    static int m_epollfd;    // 所有http对应的socket事件都被注册至统一epoll事件表中
    static int m_user_count; // 统计http连接数量
    MYSQL* mysql;            // 数据库指针
    int m_state;             // http状态, 读为0, 写为1

    int timer_flag;          // Reactor模式判断读写操作是否成功
    int improv;              // Reactor模式判断读写是否执行

private:
    static Locker m_lock;

    int m_sockfd;           // 该http所对应的socket
    sockaddr_in m_address;  // 该http所连接对应客户的ip地址
    TimerNode* m_timer;     // 定时器指针

    char m_read_buf[READ_BUFFER_SIZE];   // 读缓冲区
    long m_start_line;                   // 解析行的起始位置
    long m_read_idx;                     // 已读入的客户数据最后一个字节的下一个位置
    long m_checked_idx;                  // 当前分析的字符在读缓冲区中的位置
    char m_write_buf[WRITE_BUFFER_SIZE]; // 写缓冲区
    long m_write_idx;                    // 写缓冲区中带发送的字节数

    CHECK_STATE m_check_state;      // 主状态机当前状态
    METHOD m_method;                // http请求方法
    char *doc_root;                 // 根目录
    char *m_url;                    // 客户请求目标文件名
    char *m_version;                // http协议版本号，本服务器支持http/1.1
    char *m_host;                   // 主机名
    long m_content_length;          // http请求消息体长度
    bool m_linger;                  // http请求是否保持连接

    char m_real_file[FILENAME_LEN]; // 客户请求目标完整文件路径由 doc_root + m_url 构成
    char *m_file_address;           // 客户请求目标文件被mmap到内存中的起始位置
    struct stat m_file_stat;        // 目标文件状态，通过m_file_stat判断文件是否存在，是否为目录，是否可读等信息
    struct iovec m_iv[2];           // 使用writev写数据，m_iv[0]表示写入起始地址，m_iv[1]表示写入数据长度
    int m_iv_count;                 // 写内存块数量
    long bytes_to_send;             // 总共要发送的数据数量
    long bytes_have_send;           // 已经发送的数据数量

    char* m_content_string; //存储请求头数据
    infoMap* m_users_info;
    SQLConnPool* m_sql_conn_pool;//数据库

    int m_TRIGMode;
    int m_close_log;

public:
    HttpConn() = default;
    ~HttpConn() = default;

public:
    /*对外开放的接口*/
    void init(int sockfd, const sockaddr_in &addr, TimerNode* timer,
              char *root, infoMap* users_info, SQLConnPool* sql_conn_pool,
              int TRIGMode, int close_log);
    void close_conn(bool real_close = true);
    void process();
    bool read();
    bool write();

    sockaddr_in get_address() const { return m_address; }
    TimerNode* get_timer() const { return m_timer; }

private:
    void init();

    HTTP_CODE process_read();
    LINE_STATUS parse_line();
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();

    bool process_write(HTTP_CODE ret);
    void unmap();
    bool add_status_line(int status, const char *title);
    bool add_headers(size_t content_length);
    bool add_content(const char *content);
    bool add_response(const char *format, ...);
};

#endif //HTTP_CONN_H
