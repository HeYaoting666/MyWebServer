//
// Created by 20771 on 2023/12/26.
//

#include "config.h"

Config::Config(){
    //端口号,默认9000
    port = 9000;

    //日志写入方式，默认异步
    asyn_log_write = 1;

    //触发组合模式,默认listenfd ET + connfd ET
    trig_mode = 3;


    //优雅关闭链接，默认不使用
    OPT_LINGER = 0;

    //数据库连接池数量,默认8
    sql_num = 8;

    //线程池内的线程数量,默认8
    thread_num = 8;

    //关闭日志,默认不关闭
    close_log = 0;

    //并发模型,默认是Proactor
    actor_model = 0;
}

void Config::parse_arg(int argc, char*argv[]){
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
            case 'p':
            {
                port = atoi(optarg);
                break;
            }
            case 'l':
            {
                asyn_log_write = atoi(optarg);
                break;
            }
            case 'm':
            {
                trig_mode = atoi(optarg);
                break;
            }
            case 'o':
            {
                OPT_LINGER = atoi(optarg);
                break;
            }
            case 's':
            {
                sql_num = atoi(optarg);
                break;
            }
            case 't':
            {
                thread_num = atoi(optarg);
                break;
            }
            case 'c':
            {
                close_log = atoi(optarg);
                break;
            }
            case 'a':
            {
                actor_model = atoi(optarg);
                break;
            }
            default:
                break;
        }
    }
}