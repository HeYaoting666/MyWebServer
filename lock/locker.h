//
// Created by 20771 on 2023/12/18.
//

#ifndef LOCK_H
#define LOCK_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 信号量
class Sem {
private:
    sem_t m_sem{};
public:
    Sem() {
        if(sem_init(&m_sem, 0, 0) != 0) {
            throw std::exception();
        }
    }
    explicit Sem(int num) {
        if(sem_init(&m_sem, 0, num) != 0) {
            throw std::exception();
        }
    }
    ~Sem() {
        sem_destroy(&m_sem);
    }

    bool wait() { return sem_wait(&m_sem) == 0; }
    bool post() { return sem_post(&m_sem) == 0; }
};

// 互斥锁
class Locker {
private:
    pthread_mutex_t m_mutex{};
public:
    Locker() {
        if(pthread_mutex_init(&m_mutex, nullptr) != 0) {
            throw std::exception();
        }
    }
    ~Locker() {
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock() { return pthread_mutex_lock(&m_mutex); }
    bool unlock() { return pthread_mutex_unlock(&m_mutex); }
    pthread_mutex_t* get() { return &m_mutex; }
};

// 条件变量
class Cond {
private:
    pthread_cond_t m_cond{};
public:
    Cond() {
        if(pthread_cond_init(&m_cond, nullptr) != 0) {
            throw std::exception();
        }
    }
    ~Cond() {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *m_mutex){
        int ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t){
        int ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return ret == 0;
    }
    bool signal() { return pthread_cond_signal(&m_cond) == 0; }
    bool broadcast() { return pthread_cond_broadcast(&m_cond) == 0; }
};

#endif //LOCK_H
