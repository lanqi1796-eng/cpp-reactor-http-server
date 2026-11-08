#pragma once
#include <queue>
#include <pthread.h>
#include <cstddef>

using callback = void(*)(void*);

template<typename T>
struct Task {
    callback function = nullptr;
    T* arg = nullptr;
    Task() = default;
    Task(callback f, T* p) : function(f), arg(p) {}
};

template<typename T>
class TaskQueue {
public:
    TaskQueue();
    ~TaskQueue();
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;
    void addTask(Task<T> task);
    Task<T> takeTask();
    size_t taskNumber() const;
private:
    std::queue<Task<T>> m_taskQ;
    mutable pthread_mutex_t m_mutex;
};

#include "TaskQueue.tpp"
