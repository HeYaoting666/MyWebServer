//
// Created by 20771 on 2023/12/22.
//

#ifndef UTILS_H
#define UTILS_H

#include <cerrno>
#include <csignal>
#include <cassert>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>

//对文件描述符设置非阻塞
int setnonblocking(int fd);

//将内核事件表注册读事件，ET模式，选择开启 EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode);

//从内核事件表删除描述符
void removefd(int epollfd, int fd);

//设置信号函数
void addsig(int sig, void(handler)(int), bool restart = true);

//定时处理任务，重新定时以不断触发SIGALRM信号
template <typename T>
void timer_handler(T& timer_container, int times) {
    timer_container.tick();
    alarm(times);
}


#endif //UTILS_H
