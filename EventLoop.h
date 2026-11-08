//EventLoop.h
#pragma once

#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <cerrno>
#include <stdio.h>
#include <cstdlib>  //exit
#include <cstdio>
#include <cstdint>
#include <unistd.h>

#include <unordered_map>
#include <functional>
#include <mutex>
#include <vector>
#include <memory>
#include <atomic>

#include "Channel.h"
#define EPOLL_SIZE 1024
#define EPOLL_NUM 1024

class EventLoop {//epoll主循环
public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void loop();

    //停止事件循环
    void quit();

    //其他线程可调用，提交任务给 EventLoop
    void queueInLoop(std::function<void()> task);

    void addChannel(Channel* channel);
    void removeChannel(Channel* channel);
    void updateChannel(Channel* channel);

private:
    //eventfd 唤醒机制
    void wakeup();
    void handleWakeup();

    //执行等待中的任务
    void doPendingTasks();

private:
    int epollfd = -1; //用来保存epoll的根节点
    std::unordered_map<int,Channel*> channels;
    // 保存 fd 与 Channel 的对应关系

    //跨线程唤醒
    int wakeupFd_ = -1;//存储 eventfd() 返回的文件描述符
    std::unique_ptr<Channel> wakeupChannel_;

    //线程安全的任务队列
    std::mutex pendingMutex_;//互斥锁
    std::vector<std::function<void()>> pendingTasks_;//真正的完成任务队列

    //退出标志
    std::atomic<bool> quit_{false};//使用原子变量，避免普通 bool 的数据竞争
};

