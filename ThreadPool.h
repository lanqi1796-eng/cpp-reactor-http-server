#pragma once
#include "TaskQueue.h"
#include <vector>
#include <pthread.h>

template<typename T>
class ThreadPool {
public:
    ThreadPool(int min, int max);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 成功时线程池接管 task.arg 的所有权；失败时仍由调用者负责释放
    bool addTask(Task<T> task);
    int getBusyNum();
    int getAliveNum() const;
private:
    static void* worker(void* arg);
    TaskQueue<T> taskQ_;
    std::vector<pthread_t> threadIDs_;
    int minNum_ = 0;
    int maxNum_ = 0;
    int busyNum_ = 0;
    bool shutdown_ = false;
    pthread_mutex_t mutexPool_;
    pthread_cond_t notEmpty_;
};

#include "ThreadPool.tpp"
