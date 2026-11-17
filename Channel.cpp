#include "Channel.h"

Channel::Channel(int fd):fd(fd),events(0),revents(0) {}

int Channel::getFd() const
{
    return fd;
}

uint32_t Channel::getEvents() const
{
    return events;
}

void Channel::setEvents(uint32_t events)
{
    this->events = events;
}

void Channel::setRevents(uint32_t revents)
{
    this->revents = revents;
}

void Channel::handleEvent()
{
    std::shared_ptr<void> guard;

    //所属 Connection 存活
    if(tied_)
    {
        guard = owner_.lock();

        if(!guard)
        {
            return;
        }
    }

    //处理错误
    if(revents & EPOLLERR)
    {
        if(errorCallback)
        {
            errorCallback();
        }
        return;
    }

    //可读或对端关闭写方向
    if((events & EPOLLIN) && (revents & (EPOLLIN | EPOLLRDHUP | EPOLLHUP)))
    {
        if(readCallback)
        {
            readCallback();
            
        }
    }

    //可写
    if((events & EPOLLOUT) && (revents & EPOLLOUT))
    {
        if(writeCallback)
        {
            writeCallback();
        }
    }
}


void Channel::enableWriting()
{
    //表示保留原有的 EPOLLIN，同时增加 EPOLLOUT
    events |= EPOLLOUT;
}

void Channel::disableWriting()
{
    //表示只取消 EPOLLOUT，不影响原有读事件
    events &= ~EPOLLOUT;
}

bool Channel::isWriting() const
{
    return (events & EPOLLOUT) != 0;
}

void Channel::tie( const std::shared_ptr<void>& owner)
{
    owner_ = owner;
    tied_ = true;
}


void Channel::setReadCallback(std::function<void()> cb)
{
    readCallback = cb;
}

void Channel::setWriteCallback(std::function<void()> cb)
{
    writeCallback = cb;
}

void Channel::setErrorCallback(std::function<void()> cb)
{
    errorCallback = cb;
}