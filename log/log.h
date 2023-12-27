//
// Created by 20771 on 2023/12/18.
//

#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <string>
#include <memory.h>
#include <cstdarg>
#include "block_queue.h"

class Log {
private:
    char dir_name[128]; //路径名
    char log_name[128]; //log文件名
    int m_split_lines;  //日志最大行数
    long long m_count;  //日志行数记录
    int m_today;        //因为按天分类,记录当前时间是那一天

    int m_log_buf_size; //日志缓冲区大小
    char *m_buf;        //日志缓存空间 buf[m_log_buf_size]
    FILE *m_fp;         //打开log的文件指针

    bool m_is_async;    //是否同步标志位

    Locker m_mutex;     //互斥锁

    BlockQueue<std::string>* m_log_queue; //阻塞队列

private:
    Log() : m_count(0), m_is_async(false) {};

    ~Log() { if(m_fp != nullptr) fclose(m_fp); }

    //异步写日志方法
    void* async_write_log() {
        std::string single_log;
        while (m_log_queue->pop(single_log)) {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

public:
    // 单例模式, C++11以后, 使用局部变量懒汉不用加锁
    static Log* get_instance() {
        static Log instance;
        return &instance;
    }

    // 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char* file_name,
              int log_buf_size = 8192,
              int split_lines = 5000000,
              int max_queue_size = 0);

    // 异步写日志公有方法，调用私有方法 async_write_log
    static void* flush_log_thread(void* args) {
        Log::get_instance()->async_write_log();
    }

    //将输出内容按照标准格式整理
    void write_log(int level, const char *format, ...);

    //强制刷新缓冲区
    void flush();
};

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...)  if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...)  if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif //LOG_H
