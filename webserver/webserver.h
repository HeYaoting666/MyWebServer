//
// Created by 20771 on 2023/12/22.
//

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "../lock/locker.h"
#include "../log/log.h"
#include "../pool/sql_conn_pool.h"
#include "../pool/thread_pool.h"
#include "../http/http_conn.h"
#include "../timer/lst_timer.h"

class WebServer {
public:
    typedef std::unordered_map<std::string, std::string> infoMap;

    static const int MAX_FD = 65536;           //最大文件描述符
    static const int MAX_EVENT_NUMBER = 10000; //最大事件数
    static const int TIMESLOT = 5;             //最小超时单位

public:
    WebServer(int port,
              std::string log_save_path,
              std::string user,
              std::string pass_word,
              std::string data_base_name,
              int asyn_log_write,
              int opt_linger,
              int trig_mode,
              int sql_num,
              int thread_num,
              int close_log,
              int actor_model);

    ~WebServer();

private:
    //基础
    int m_port;             // 监听端口
    char m_root[128];       // 资源根目录
    int m_asyn_log_write;   // 异步写日志，0为异步写，1为同步写
    int m_close_log;        // 写日志标志，0为打开，1为关闭
    int m_actor_model;      // 服务器架构模式，0为Proactor模式，1为Reactor模式
    HttpConn* http_users;   // http连接资源

    static int m_pipefd[2]; // 管道文件描述符，用来传递信号信息
    int m_OPT_LINGER;


    //日志相关
    std::string m_log_save_path;  // 日志存储路径

    //数据库相关
    SQLConnPool* m_sql_conn_pool;  //数据库连接池
    std::string m_user;            //登陆数据库用户名
    std::string m_pass_word;       //登陆数据库密码
    std::string m_data_base_name;  //使用数据库名
    int m_max_sql_num;             //连接池最大数量
    infoMap m_users_info;          //用户信息<用户名, 密码>

    //线程池相关
    ThreadPool<HttpConn>* m_thread_pool;
    int m_thread_num;

    //epoll_event相关
    int m_epollfd;
    int m_listenfd;
    epoll_event events[MAX_EVENT_NUMBER];

    int m_TRIGMode;
    int m_LISTEN_TRIGMode;
    int m_CONN_TRIGMode;

    //定时器相关
    SortTimerList m_timer_list;

public:
    void event_listen();
    void event_loop();

private:
    //设置监听套接字和连接套接字模式
    void set_trig_mode();

    //初始化日志模块
    void init_log() const;

    //初始化数据库连接池
    void init_sql_pool();
    void get_mysql_result(SQLConnPool* sql_conn_pool);

    //初始化线程池
    void init_thread_pool();

    // 处理新客户连接
    bool deal_client_connect();

    // 处理信号事件
    bool deal_with_signal(bool& timeout, bool& stop_server);

    // 处理读事件
    void deal_with_read(int connfd);

    // 处理写事件
    void deal_with_write(int connfd);

    // 初始化客户连接资源
    void init_client_conn(int connfd, struct sockaddr_in client_address);

    // 删除客户
    void close_client_conn(int sockfd);

    // 调整定时器
    void adjust_timer(int sockfd);

    //信号回调函数
    static void sig_handler(int sig) {
        int save_errno = errno;
        send(m_pipefd[1], (char*)&sig, 1, 0);
        errno = save_errno;
    }
    // 定时回调函数
    static void timer_call_back(HttpConn* user) {
        int m_close_log = 0;
        LOG_INFO("Time OUt")
        user->close_conn();
    }
};

#endif //WEBSERVER_H
