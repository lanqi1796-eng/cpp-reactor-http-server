#include "Connection.h"

int Connection::getFd() const
{
    return fd_;
}

void Connection::setMessageCallback(MessageCallback cb)
{
    messageCallback_ = cb;
}

void Connection::setCloseCallback(CloseCallback cb)
{
    closeCallback_ = cb;
}



Connection::Connection(EventLoop* loop,int fd):
    loop_(loop),fd_(fd),channel_(fd),closed_(false)
{
    // 初始只监听读事件
    channel_.setEvents(EPOLLIN);

    channel_.setReadCallback([this]()
    {
        handleRead();
    });

    //写回调
    channel_.setWriteCallback([this]()
    {
        handleWrite();
    });

    //错误回调
    channel_.setErrorCallback([this]()
    {
        handleClose();
    });
}


void Connection::establish()
{
    channel_.tie(shared_from_this());
    loop_->addChannel(&channel_);
}


void Connection::closeConnection()
{
    handleClose();
}


//实现纯缓冲式send()
void Connection::send(const std::string& data)
{
    if(closed_ || closeAfterWrite_)
    {
        return;
    }

    if(data.empty())
    {
        return;
    }

    //放进待发送缓冲区
    outputBuffer_.append(data);

    //如果尚未监听 EPOLLOUT，就开始监听
    if(!channel_.isWriting())
    {
        channel_.enableWriting();

        // 通知 EventLoop 更新 epoll
        loop_->updateChannel(&channel_);
    }
}



void Connection::shutdownAfterWrite()
{
    if(closed_)
    {
        return;
    }

    closeAfterWrite_ = true;

    // 没有待发送数据，可以直接关闭
    if(outputBuffer_.empty())
    {
        closeConnection();
        return;
    }

    // 已经决定关闭，不再接收新的 HTTP 请求
    channel_.setEvents( channel_.getEvents() & ~EPOLLIN );

    // outputBuffer_ 非空，EPOLLOUT 应当已开启
    loop_->updateChannel(&channel_);
}



void Connection::handleRead()
{
    char buf[4096];
    ssize_t n;

    //EINTR时重试
    do
    {
        n = ::recv(fd_,buf,sizeof(buf),0);
    } 
    while (n<0 && errno == EINTR);

    //成功读取数据
    if(n>0)
    {
        inputBuffer_.append(buf,static_cast<size_t>(n));

        if(messageCallback_)
        {
            messageCallback_(this,inputBuffer_);
        }

        // 不再访问成员，回调可能触发连接关闭
        return;
    }

    //对端关闭写方向
    if(n==0)
    {
        shutdownAfterWrite();
        return;
    }

    //暂时没有数据可读
    if(errno==EAGAIN || errno==EWOULDBLOCK)
    {
        return;
    }
    
    //其他读取错误
    perror("recv error");
    closeConnection();
}



void Connection::handleClose()
{
    //标志close,以防重复关闭
    if(closed_)
    {
        return;
    }
    closed_ = true;

    std::cout<<"client "<<fd_<<" is disconnecting"<<std::endl;

    loop_->removeChannel(&channel_);

    Close(fd_);

    if(closeCallback_)
    {
        closeCallback_(this);
    }
}


void Connection::handleWrite()
{
    if(closed_)
    {
        return;
    }

    // 防御性检查
    if(outputBuffer_.empty())
    {
        if(closeAfterWrite_)
        {
            closeConnection();
            return;
        }

        if(channel_.isWriting())
        {
            channel_.disableWriting();
            loop_->updateChannel(&channel_);
        }

        return;
    }

    //将缓冲区的数据交给内核
    ssize_t n;

    do
    {
        n=::send(fd_,
                 outputBuffer_.data(),
                 outputBuffer_.size(),
                 MSG_NOSIGNAL //避免进程被默认终止
        );
    } 
    while (n<0 && errno==EINTR);

    //成功发送了n字节
    if(n>0)
    {
        outputBuffer_.erase(0,static_cast<size_t>(n));
    }//从缓冲区开头删除已经发送的部分，避免下次重复发送

    //没有成功发送但是连接是正常的
    else if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK))
    {
        return;
    }

    //其他结果就停止发送并关闭连接
    else if(n<0)
    {
        //继续等待发送通常没有意义，所以记录错误并清理连接
        perror("send error");
        closeConnection();
        return;
    }

    else
    {
        // 发送没有任何进展，避免反复触发可写事件
        closeConnection();
        return;
    }

    //成功发送后，检查是否还有剩余数据
    if(!outputBuffer_.empty())
    {
        return;
    }

    //已经全部发送完毕并要求关闭时进行关闭
    if(closeAfterWrite_)
    {
        //HTTP要求发送完后关闭
        closeConnection();
        return;
    }

    // 长连接：停止监听写事件，继续监听读事件
    channel_.disableWriting();
    loop_->updateChannel(&channel_);
}

void Connection::pauseReading()
{
    if(closed_)
    {
        return;
    }

    uint32_t events = channel_.getEvents();

    if(events & EPOLLIN)
    {
        channel_.setEvents(events & ~EPOLLIN);
        loop_->updateChannel(&channel_);
    }
}

void Connection::resumeReading()
{
    if(closed_ || closeAfterWrite_)
    {
        return;
    }

    uint32_t events = channel_.getEvents();

    if(!(events & EPOLLIN))
    {
        channel_.setEvents(events | EPOLLIN);
        loop_->updateChannel(&channel_);
    }
}

void Connection::processBufferedInput()
{
    if(closed_ || closeAfterWrite_)
    {
        return;
    }

    if(!(channel_.getEvents() & EPOLLIN))
    {
        return;
    }

    if(!inputBuffer_.empty() && messageCallback_)
    {
        messageCallback_(this, inputBuffer_);
    }
}



void Connection::consumeInput(size_t len)
{
    inputBuffer_.erase(0,len);
}