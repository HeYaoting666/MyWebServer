//
// Created by 20771 on 2023/12/21.
//

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <list>
#include <memory>
#include <exception>
#include <pthread.h>
#include "sql_conn_pool.h"
#include "../lock/locker.h"

template <typename T>
class ThreadPool {
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    ThreadPool(int actor_model, int thread_number, int max_requests = 10000);
    ~ThreadPool() { delete[] m_threads; }
    bool append(T* request, int state);
    bool append(T* request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void* worker(void* arg);
    void run();

private:
    int m_thread_number;         //线程池中的线程数
    int m_max_requests;          //请求队列中允许的最大请求数
    pthread_t* m_threads;        //描述线程池的数组，其大小为m_thread_number

    int m_actor_model;           //模型切换

    std::list<T*> m_work_queue;  //请求队列, 生产者消费者模型
    Locker m_queue_locker;       //保护请求队列的互斥锁
    Sem m_queue_stat;            //是否有任务需要处理
};

template<typename T>
ThreadPool<T>::ThreadPool(int actor_model, int thread_number, int max_requests)
    :m_actor_model(actor_model),
    m_thread_number(thread_number),
    m_max_requests(max_requests) {

    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();

    m_threads = new pthread_t [thread_number]; // 动态创建线程池
    if(!m_threads)
        throw std::exception();

    for(int i = 0; i < m_thread_number; ++i) {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
bool ThreadPool<T>::append(T* request, int state) {
    m_queue_locker.lock();
    if(m_work_queue.size() >= m_max_requests) {
        m_queue_locker.unlock();
        return false;
    }
    request->m_state = state;
    m_work_queue.push_back(request);
    m_queue_locker.unlock();
    m_queue_stat.post();
    return true;
}

template<typename T>
bool ThreadPool<T>::append(T* request) {
    m_queue_locker.lock();
    if(m_work_queue.size() >= m_max_requests) {
        m_queue_locker.unlock();
        return false;
    }
    m_work_queue.push_back(request);
    m_queue_locker.unlock();
    m_queue_stat.post();
    return true;
}

template<typename T>
void *ThreadPool<T>::worker(void *arg) {
    auto pool = (ThreadPool*)arg;
    pool->run();
}

template<typename T>
void ThreadPool<T>::run() {
    while(true) {
        m_queue_stat.wait();
        m_queue_locker.lock();
        if (m_work_queue.empty()) // 队列没有任务请求则跳过
        {
            m_queue_locker.unlock();
            continue;
        }
        // 有任务时获取任务请求
        T* request = m_work_queue.front();
        m_work_queue.pop_front();
        m_queue_locker.unlock();
        if(!request) continue;

        // Reactor模式，I/O操作在从线程中完成
        if(m_actor_model == 1) {
            // 读状态
            if(request->m_state == 0) {
                if(request->read()) {
                    request->process();
                }
                else {
                    request->timer_flag = 1;
                }
                request->improv = 1;
            }
            // 写状态
            else {
                if (!request->write())
                    request->timer_flag = 1;

                request->improv = 1;
            }
        }
        // Proactor模式，I/O操作在主线程中完成，从线程只负责逻辑处理
        else {
            request->process();
        }
    }
}

#endif //THREAD_POOL_H
