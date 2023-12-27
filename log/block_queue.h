/*************************************************************
* Created by 20771 on 2023/12/18.
* 循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size;
* 线程安全，每个操作前都要先加互斥锁，操作完后，再解锁
**************************************************************/

#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include<sys/time.h>
#include "../lock/locker.h"

template <typename T>
class BlockQueue {
private:
    T* m_array;           // 阻塞队列
    int m_size;           // 队列当前大小
    int m_max_size; // 队列最大容量
    int m_front;          // 队头位置
    int m_back;           // 队尾位置

    Locker m_mutex; // 互斥锁
    Cond m_cond;    // 条件变量

public:
    explicit BlockQueue(int max_size = 1000)
            : m_size(0), m_max_size(max_size), m_front(-1), m_back(-1)
    {
        if(max_size <= 0) exit(-1);
        m_array = new T[max_size];
    }

    ~BlockQueue() {
        m_mutex.lock();
        if(m_array != nullptr) {
            delete []m_array;
            m_array = nullptr;
        }
        m_mutex.unlock();
    }

    void clear() {
        m_mutex.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    bool full() {
        m_mutex.lock();
        if(m_size >= m_max_size) {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    bool empty() {
        m_mutex.lock();
        if(m_size == 0) {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    bool front(T& value) {
        m_mutex.lock();
        if(m_size == 0) {
            m_mutex.unlock();
            return false;
        }

        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    bool back(T& value) {
        m_mutex.lock();
        if(m_size == 0) {
            m_mutex.unlock();
            return false;
        }

        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    int size()
    {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_size;

        m_mutex.unlock();
        return tmp;
    }

    int max_size()
    {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_max_size;

        m_mutex.unlock();
        return tmp;
    }

    // 往队列添加元素，需要将所有使用队列的线程先唤醒
    // 当有元素push进队列,相当于生产者生产了一个元素
    // 若当前没有线程等待条件变量,则唤醒无意义
    bool push(const T& item) {
        m_mutex.lock();
        // 队列已满则插入失败,唤醒使用队列的线程
        if(m_size >= m_max_size) {
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;
        ++m_size;

        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    bool pop(T& item) {
        m_mutex.lock();
        // pop时,如果当前队列没有元素,将会等待条件变量
        while (m_size <= 0) {
            if(!m_cond.wait(m_mutex.get())) {
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        --m_size;

        m_mutex.unlock();
        return true;
    }

    bool pop(T& item, int ms_timeout) {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, nullptr);

        m_mutex.lock();
        if (m_size <= 0) {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (m_cond.timewait(m_mutex.get(), t) != 0)
            {
                m_mutex.unlock();
                return false;
            }
        }
        if(m_size <= 0) {
            m_mutex.unlock();
            return false;
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        --m_size;

        m_mutex.unlock();
        return true;
    }
};

#endif //BLOCK_QUEUE_H
