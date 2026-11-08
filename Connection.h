//Connection
#pragma once
#include "EventLoop.h"
#include "wrap.h"

#include <string>
#include <functional>
#include <iostream>
#include <memory>
#include <sys/socket.h>//套接字接口
#include <cerrno>//错误码变量
#include <cstdio>//C风格输入输出

class Connection: public std::enable_shared_from_this<Connection>
{
public:
    using MessageCallback =
        std::function<void(Connection*, const std::string&)>;

    using CloseCallback =
        std::function<void(Connection*)>;

public:
    Connection(EventLoop* loop,int fd);

    void establish();

    int getFd() const;

    void setMessageCallback(MessageCallback cb);
    void setCloseCallback(CloseCallback cb);

    //告诉Connection前len个字节已经处理完了，可以清理了
    void consumeInput(size_t len);

    //支持主动关闭的接口
    void closeConnection();

    void send(const std::string& data);//待发送数据加入输出缓存，必要时启用 EPOLLOUT
    void shutdownAfterWrite();//标记响应发送完成后关闭连接

public:
    // 暂停读取客户端的新数据
    void pauseReading();

    // 恢复监听 EPOLLIN
    void resumeReading();

    // 继续处理 inputBuffer_ 中已经收到的数据
    void processBufferedInput();

private:
    void handleRead();//真正把数据从socket读出来
    void handleClose();

    void handleWrite();//真正发送缓冲区中的数据

private:
    EventLoop* loop_;
    int fd_;
    Channel channel_;

    std::string inputBuffer_;

    MessageCallback messageCallback_;
    CloseCallback closeCallback_;

    //bool关闭,做状态保护
    bool closed_ = false;

    std::string outputBuffer_;
    bool closeAfterWrite_ = false;
};



