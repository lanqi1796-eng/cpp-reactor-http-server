//Channel.h
#pragma once
#include <functional>
#include <cstdint>
#include <sys/epoll.h>
#include <memory>

class Channel
{
public:
    Channel(int fd);

public:
    int getFd() const;
    uint32_t getEvents() const;

    void setEvents(uint32_t events);
    void setRevents(uint32_t events);

    void setReadCallback(std::function<void()> cb);
    void setWriteCallback(std::function<void()> cb);
    void setErrorCallback(std::function<void()> cb);

    void handleEvent();

    //操作位运算开关读写事件
    void enableWriting();
    void disableWriting();
    bool isWriting() const;

    // 让处理事件时保持所属 Connection 存活
    void tie(const std::shared_ptr<void>& owner);

private:
    int fd;

    uint32_t events;
    uint32_t revents;

    std::function<void()> readCallback;
    std::function<void()> writeCallback;
    std::function<void()> errorCallback;

    std::weak_ptr<void> owner_;
    bool tied_ = false;

};



