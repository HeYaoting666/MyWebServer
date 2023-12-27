//
// Created by 20771 on 2023/12/19.
//

#ifndef SQL_CONN_POOL_H
#define SQL_CONN_POOL_H

#include <string>
#include <list>
#include <mysql/mysql.h>
#include "../lock/locker.h"
#include "../log/log.h"

class SQLConnPool {
private:
    int m_close_log;	         //日志开关 0 为关闭，1为打开

    std::string m_url;			 //主机地址
    int m_port;		             //数据库端口号
    std::string m_user;		     //登陆数据库用户名
    std::string m_pass_word;	 //登陆数据库密码
    std::string m_data_base_name;//使用数据库名

    int m_max_conn;              //最大连接数
    int m_cur_conn;              //当前已使用的连接数
    int m_free_conn;             //当前空闲的连接数
    std::list<MYSQL *> connList; //连接池

    Locker locker;
    Sem reserve;

private:
    SQLConnPool() : m_free_conn(0), m_cur_conn(0), m_max_conn(0), m_close_log(0), m_port(3306){};
    ~SQLConnPool() { destroy_pool(); };

public:
    // 单例模式
    static SQLConnPool* get_instance(){
        static SQLConnPool instance;
        return &instance;
    }

    void init(const std::string& url, const std::string& user, const std::string& pass_word,
              const std::string& data_base_name, int port,
              int max_conn, int close_log);

    MYSQL* get_connection();			  //获取数据库连接
    bool release_connection(MYSQL *conn); //释放连接
    void destroy_pool();				  //销毁所有连接

    int get_free_conn() const { return m_free_conn; }  //获取空闲连接数
    int get_cur_conn() const { return m_cur_conn; }	   //获取已使用连接数
    int get_max_conn() const { return m_max_conn; }	   //获取最大连接数

};

class ConnectionRAII {
private:
    MYSQL **conRAII;
    SQLConnPool *poolRAII;
public:
    ConnectionRAII(MYSQL **mysql_conn, SQLConnPool *conn_pool) {
        *mysql_conn = conn_pool->get_connection();

        conRAII = mysql_conn;
        poolRAII = conn_pool;
    }
    ~ConnectionRAII() {
        poolRAII->release_connection(*conRAII);
        *conRAII = nullptr;
    }
};

#endif //SQL_CONN_POOL_H
