//
// Created by 20771 on 2023/12/26.
//

#ifndef CONFIG_H
#define CONFIG_H

#include <getopt.h>
#include <cstdlib>

class Config
{
public:
    Config();
    ~Config()= default;

    void parse_arg(int argc, char*argv[]);

    //端口号
    int port;

    //日志写入方式
    int asyn_log_write;

    //触发组合模式
    int trig_mode;

    //优雅关闭链接
    int OPT_LINGER;

    //数据库连接池数量
    int sql_num;

    //线程池内的线程数量
    int thread_num;

    //是否关闭日志
    int close_log;

    //并发模型选择
    int actor_model;
};

#endif //CONFIG_H
