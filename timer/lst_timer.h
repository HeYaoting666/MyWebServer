//
// Created by 20771 on 2023/12/22.
//
#ifndef LST_TIMER_H
#define LST_TIMER_H

#include "../http/http_conn.h"

struct TimerNode {
public:
    TimerNode(time_t expire_time, HttpConn* client, void (*cb_f)(HttpConn*)) :
        expire(expire_time), user_data(client), cb_func(cb_f),
        prev(nullptr), next(nullptr) {}

public:
    time_t expire;
    HttpConn* user_data;
    TimerNode* prev;
    TimerNode* next;

    void (*cb_func)(HttpConn*){};
};

class SortTimerList {
private:
    TimerNode *head;
    TimerNode *tail;

public:
    SortTimerList() : head(nullptr), tail(nullptr) {}
    ~SortTimerList() {
        TimerNode* tmp = head;
        while(tmp) {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

public:
    void add_timer(TimerNode* timer);
    void adjust_timer(TimerNode* timer);
    void del_timer(TimerNode* timer);
    void tick();

private:
    void add_timer(TimerNode* timer, TimerNode* lst_head);
};

#endif //LST_TIMER_H
