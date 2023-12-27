//
// Created by 20771 on 2023/12/19.
//
#include "sql_conn_pool.h"

//构造初始化
void SQLConnPool::init(const std::string& url, const std::string& user, const std::string& pass_word,
                       const std::string& data_base_name, int port,
                       int max_conn, int close_log) {
    m_url = url;
    m_port = port;
    m_user = user;
    m_pass_word = pass_word;
    m_data_base_name = data_base_name;
    m_max_conn = max_conn;
    m_close_log = close_log;

    for(int i = 0; i < m_max_conn; ++i) {
        MYSQL* conn = nullptr;
        conn = mysql_init(conn);

        if(conn == nullptr) {
            LOG_ERROR("MySQL Init Error");
            exit(1);
        }

        conn = mysql_real_connect(conn, m_url.c_str(), m_user.c_str(), m_pass_word.c_str(),
                                  m_data_base_name.c_str(), m_port, nullptr, 0);
        if(conn == nullptr) {
            LOG_ERROR("MySQL Connection Error");
            exit(1);
        } else {
            LOG_INFO("MySQL Connection Successful");
        }

        connList.push_back(conn);
        ++m_free_conn;
    }
    reserve = Sem(m_free_conn);
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL* SQLConnPool::get_connection() {
    MYSQL* conn = nullptr;
    if(connList.empty())
        return nullptr;

    reserve.wait();
    locker.lock();

    conn = connList.front();
    connList.pop_front();
    --m_free_conn;
    ++m_cur_conn;

    locker.unlock();

    LOG_INFO("Get One MySQL Connection");
    return conn;
}

//释放当前使用的连接
bool SQLConnPool::release_connection(MYSQL *conn) {
    if(conn == nullptr)
        return false;

    locker.lock();

    connList.push_back(conn);
    ++m_free_conn;
    --m_cur_conn;

    locker.unlock();
    reserve.post();

    LOG_INFO("Release One MySQL Connection");
    return true;
}

void SQLConnPool::destroy_pool() {
    locker.lock();
    if(!connList.empty()) {
        for(auto & it : connList) {
            mysql_close(it);
        }
        m_cur_conn = 0;
        m_free_conn = 0;
        m_max_conn = 0;
        connList.clear();
    }
    locker.unlock();
    LOG_INFO("All MySQL Connections are destroyed");
}

