//
// Created by 20771 on 2023/12/22.
//
#include "lst_timer.h"

void SortTimerList::add_timer(TimerNode* timer) {
    if(!timer) return;

    if(!head) {
        head = tail = timer;
        return;
    }

    // 头部插入
    if(timer->expire <= head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }

    // 尾部插入
    if(timer->expire >= tail->expire) {
        timer->prev = tail;
        tail->next = timer;
        tail = timer;
        return;
    }

    // 即不在头也不在尾，寻找插入位置
    add_timer(timer, head);
}

void SortTimerList::del_timer(TimerNode* timer) {
    if(!timer) return;

    if(head == tail) {
        delete timer;
        head = nullptr;
        tail = nullptr;
        return;
    }

    if(timer == head) {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }

    if(timer == tail) {
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }

    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void SortTimerList::adjust_timer(TimerNode* timer) {
    if(!timer) return;

    TimerNode* tmp = timer->next;
    if(!tmp || (timer->expire <= tmp->expire))
        return;

    if(timer == head) {
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer( timer, head );
    }
    else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer( timer, timer->next );
    }

}

void SortTimerList::tick() {
    if(!head) return;

    time_t cur_time = time(nullptr);
    TimerNode* tmp = head;
    while(tmp) {
        if(cur_time < tmp->expire) break;

        tmp->cb_func(tmp->user_data);
        del_timer(tmp);
        tmp = head;
    }

}

void SortTimerList::add_timer(TimerNode* timer, TimerNode* lst_head) {
    TimerNode* prev = lst_head;
    TimerNode* cur = lst_head->next;
    while(cur) {
        if(timer->expire <= cur->expire) {
            prev->next = timer;
            timer->next = cur;

            cur->prev = timer;
            timer->prev = prev;

            break;
        }
        prev = cur;
        cur = cur->next;
    }
    if(!cur) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = nullptr;
        tail = timer;
    }
}
