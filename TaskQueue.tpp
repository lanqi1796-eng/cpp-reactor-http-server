#include <system_error>
#include "TaskQueue.h"

template<typename T>
TaskQueue<T>::TaskQueue() {
    int rc = pthread_mutex_init(&m_mutex, nullptr);
    if(rc != 0) throw std::system_error(rc, std::generic_category(), "TaskQueue mutex_init");
}

template<typename T>
TaskQueue<T>::~TaskQueue() { pthread_mutex_destroy(&m_mutex); }

template<typename T>
void TaskQueue<T>::addTask(Task<T> task) {
    pthread_mutex_lock(&m_mutex);
    try {
        m_taskQ.push(task);
    } catch (...) {
        pthread_mutex_unlock(&m_mutex);
        throw;
    }
    pthread_mutex_unlock(&m_mutex);
}

template<typename T>
Task<T> TaskQueue<T>::takeTask() {
    pthread_mutex_lock(&m_mutex);
    Task<T> result;
    if(!m_taskQ.empty()) {
        result = m_taskQ.front();
        m_taskQ.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return result;
}

template<typename T>
size_t TaskQueue<T>::taskNumber() const {
    pthread_mutex_lock(&m_mutex);
    size_t n = m_taskQ.size();
    pthread_mutex_unlock(&m_mutex);
    return n;
}
