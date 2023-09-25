#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class sem
{
public:
    sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0)// 初始化一个信号量(用于进程间的同步，以控制对共享资源的访问)
        {                               // &m_sem指向信号量变量的指针，'0'表示信号量的共享性，'0'表示初始信号量为0
            throw std::exception();
        }
    }
    sem(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0) //初始信号量设为num
        {
            throw std::exception();
        }
    }
    ~sem()
    {
        sem_destroy(&m_sem);// 销毁信号量
    }
    bool wait()
    {
        return sem_wait(&m_sem) == 0; // 等待信号量的值变为非负数，用于协调多个线程之间的操作
    }
    bool post()
    {
        return sem_post(&m_sem) == 0; // 增加信号量的值
    }
private:
    sem_t m_sem;
};

class locker
{
public:
    locker() // 用于确保多个线程在访问共享资源时不会发生竞态条件
    { 
        if (pthread_mutex_init(&m_mutex, NULL) != 0) // 初始化互斥锁
        {
            throw std::exception(); //初始化失败
        }
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);  // 销毁互斥锁
    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0; // 锁定互斥锁
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0; //解锁
    }
    pthread_mutex_t *get() // 允许外部代码获取对互斥锁m_mutex的访问
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;  //互斥锁变量指针
};

class cond
{
public:
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0) // 初始化条件变量
        {
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond); // 销毁条件变量
    }
    bool wait(pthread_mutex_t *m_mutex)
    { // 用于线程之间的同步，其中一个线程等待某个条件的发生，而其他线程在满足条件时通知等待线程
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, m_mutex);// 等待条件变量的发生，并传入互斥锁，
        return ret == 0;   // 在等待期间，互斥锁会被释放，允许其他线程进入临界区
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);//等待条件变量的发生，但在要在超时时间内返回
        return ret == 0;
    }
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0; //发送信号给等待在条件变量'm_cond'上的一个线程，以唤醒它
    }                                             // 允许一个线程通知其他线程某个条件以满足
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0; // 广播信号给等待在条件变量'm_cond'上的所有线程，以唤醒它们
    }
private:
    pthread_cond_t m_cond; // 条件变量是一种线程同步机制，允许线程等待某个条件的发生，以及在条件满足时被唤醒
};
#endif