#include <memory>
#include <stdexcept>
#include <system_error>
#include <iostream>

#include "ThreadPool.h"

template<typename T>
ThreadPool<T>::ThreadPool(int min, int max) : minNum_(min), maxNum_(max) {
    if(min <= 0 || max < min) throw std::invalid_argument("invalid thread pool size");

    int rc = pthread_mutex_init(&mutexPool_, nullptr);
    if(rc != 0) throw std::system_error(rc, std::generic_category(), "pool mutex_init");

    rc = pthread_cond_init(&notEmpty_, nullptr);
    if(rc != 0) {
        pthread_mutex_destroy(&mutexPool_);
        throw std::system_error(rc, std::generic_category(), "pool cond_init");
    }

    try {
        threadIDs_.resize(static_cast<size_t>(min));
    } catch (...) {
        pthread_cond_destroy(&notEmpty_);
        pthread_mutex_destroy(&mutexPool_);
        throw;
    }

    size_t created = 0;
    for (; created < threadIDs_.size(); ++created) {
        rc = pthread_create(&threadIDs_[created], nullptr, &ThreadPool<T>::worker, this);
        if(rc != 0) break;
    }

    if(rc != 0) {
        pthread_mutex_lock(&mutexPool_);
        shutdown_ = true;
        pthread_cond_broadcast(&notEmpty_);
        pthread_mutex_unlock(&mutexPool_);
        for(size_t i = 0; i < created; ++i) pthread_join(threadIDs_[i], nullptr);
        pthread_cond_destroy(&notEmpty_);
        pthread_mutex_destroy(&mutexPool_);
        throw std::system_error(rc, std::generic_category(), "pthread_create");
    }
}

template<typename T>
ThreadPool<T>::~ThreadPool() {
    pthread_mutex_lock(&mutexPool_);
    shutdown_ = true;
    pthread_cond_broadcast(&notEmpty_);
    pthread_mutex_unlock(&mutexPool_);

    for(pthread_t tid : threadIDs_) pthread_join(tid, nullptr);

    pthread_cond_destroy(&notEmpty_);
    pthread_mutex_destroy(&mutexPool_);
}

template<typename T>
bool ThreadPool<T>::addTask(Task<T> task) {
    if(task.function == nullptr) return false;

    pthread_mutex_lock(&mutexPool_);
    if(shutdown_) {
        pthread_mutex_unlock(&mutexPool_);
        return false;
    }

    try {
        taskQ_.addTask(task);
    } catch (...) {
        pthread_mutex_unlock(&mutexPool_);
        throw;
    }
    pthread_cond_signal(&notEmpty_);
    pthread_mutex_unlock(&mutexPool_);
    return true;
}

template<typename T>
int ThreadPool<T>::getBusyNum() {
    pthread_mutex_lock(&mutexPool_);
    int n = busyNum_;
    pthread_mutex_unlock(&mutexPool_);
    return n;
}

template<typename T>
int ThreadPool<T>::getAliveNum() const {
    return static_cast<int>(threadIDs_.size());
}

template<typename T>
void* ThreadPool<T>::worker(void* arg) {
    auto* pool = static_cast<ThreadPool<T>*>(arg);

    while(true) {
        pthread_mutex_lock(&pool->mutexPool_);

        while(pool->taskQ_.taskNumber() == 0 && !pool->shutdown_) {
            pthread_cond_wait(&pool->notEmpty_, &pool->mutexPool_);
        }

        if(pool->shutdown_ && pool->taskQ_.taskNumber() == 0) {
            pthread_mutex_unlock(&pool->mutexPool_);
            return nullptr;
        }

        Task<T> task = pool->taskQ_.takeTask();
        ++pool->busyNum_;
        pthread_mutex_unlock(&pool->mutexPool_);

        std::unique_ptr<T> ownedArg(task.arg); // 调用成功后，自动释放任务参数
        try {
            if(task.function) task.function(ownedArg.get());
        } catch(const std::exception& e) {
            std::cerr << "worker exception: " << e.what() << '\n';
        } catch(...) {
            std::cerr << "worker exception: unknown\n";
        }

        pthread_mutex_lock(&pool->mutexPool_);
        --pool->busyNum_;
        pthread_mutex_unlock(&pool->mutexPool_);
    }
}
