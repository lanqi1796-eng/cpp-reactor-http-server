#include "EventLoop.h"
#include <utility>

EventLoop::EventLoop()
{
    epollfd = epoll_create(EPOLL_CLOEXEC);

    if(epollfd == -1)
    {
        perror("epoll create error");
        exit(1);
    }

    //创建非阻塞eventfd
    wakeupFd_ = eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);

    if(wakeupFd_ == -1)
    {
        perror("eventfd");
        close(epollfd);
        std::exit(1);
    }

    //给 eventfd 创建 Channel
    wakeupChannel_ = std::make_unique<Channel>(wakeupFd_);

    //eventfd 只监听读事件
    wakeupChannel_->setEvents(EPOLLIN);

    //设置读回调
    wakeupChannel_->setReadCallback([this]()
    {
        handleWakeup();
    });

    //加入现有 epoll 监听
    addChannel(wakeupChannel_.get());

    //确认注册成功
    if(channels.find(wakeupFd_) == channels.end())
    {
        std::fprintf(stderr, "register eventfd failed\n");
        std::exit(1);
    }
}

EventLoop::~EventLoop()
{
    // 先从 epoll 移除 wakeupChannel
    if(wakeupChannel_)
    {
        removeChannel(wakeupChannel_.get());
        wakeupChannel_.reset();
    }

    // 再关闭 eventfd
    if(wakeupFd_ != -1)
    {
        close(wakeupFd_);
    }

    // 最后关闭 epollfd
    if(epollfd != -1)
    {
        close(epollfd);
    }
}

void EventLoop::queueInLoop( std::function<void()> task)
{
    //加锁保护任务队列
    {
        std::lock_guard<std::mutex> lock(
            pendingMutex_
        );

        pendingTasks_.push_back(
            std::move(task)
        );
    }

    //锁释放以后，通知 EventLoop
    wakeup();
}

void EventLoop::wakeup()
{
    uint64_t one = 1;

    ssize_t n;

    do
    {
        n = ::write(
            wakeupFd_,
            &one,
            sizeof(one)
        );
    }
    while(n == -1 && errno == EINTR);

    if(n != static_cast<ssize_t>(sizeof(one)) && !(n == -1 && errno == EAGAIN))
    {
        perror("eventfd write");
    }
}

void EventLoop::handleWakeup()
{
    uint64_t value;

    while(true)
    {
        ssize_t n = ::read(
            wakeupFd_,
            &value,
            sizeof(value)
        );

        // 成功读取通知
        if(n == sizeof(value))
        {
            continue;
        }

        // 被信号中断，重试
        if(n == -1 && errno == EINTR)
        {
            continue;
        }

        // 暂时没有通知
        if(n == -1 && (errno == EAGAIN ||
                       errno == EWOULDBLOCK))
        {
            return;
        }

        // 其他错误
        perror("eventfd read");
        return;
    }
}

void EventLoop::doPendingTasks()
{
    std::vector<std::function<void()>> tasks;

    // 只在交换任务队列时持锁
    {
        std::lock_guard<std::mutex> lock(
            pendingMutex_
        );

        tasks.swap(pendingTasks_);
    }

    // 不持有互斥锁，依次执行任务
    for(auto& task : tasks)
    {
        if(task)
        {
            task();
        }
    }
}

void EventLoop::addChannel(Channel* channel)
{
    struct epoll_event ev;

    int fd = channel->getFd();
    ev.events = channel->getEvents();
    ev.data.fd = fd;

    if(epoll_ctl(epollfd,
                  EPOLL_CTL_ADD,
                  fd,
                  &ev) == -1)
    {//第三个参数fd是用来干什么的？是用来表示监听的套接字的
        perror("ADD_ERROR");
        return;
    }

    channels[fd] = channel;
}


void EventLoop::removeChannel(Channel* channel)
{
    int fd = channel->getFd();

    if(epoll_ctl(epollfd,
                  EPOLL_CTL_DEL,
                  fd,
                  0) == -1)
    {
        perror("DEL_ERROR");
        return;
    }

    channels.erase(fd);
}


void EventLoop::updateChannel(Channel* channel)
{
    struct epoll_event ev;

    int fd = channel->getFd();
    ev.events = channel->getEvents();
    ev.data.fd = fd;

    if(epoll_ctl(epollfd,
                  EPOLL_CTL_MOD,
                  fd,
                  &ev) == -1)
    {
        perror("MOD_ERROR");
        return;
    }

    channels[fd] = channel;
}

void EventLoop::loop()
{
    struct epoll_event events[EPOLL_NUM];

    while(!quit_.load())
    {
        int n = epoll_wait( epollfd,
                            events,
                            EPOLL_NUM,
                            -1
        );

        if(n == -1)
        {
            if(errno == EINTR)
            {
                continue;
            }
            perror("epoll wait");
            break;
        }

        //处理就绪事件
        for(int i = 0; i < n; i++)
        {
            int fd = events[i].data.fd;
            auto it = channels.find(fd);

            if(it == channels.end())
            {
                continue;
            }

            Channel* channel = it->second;

            channel->setRevents(
                events[i].events
            );
            channel->handleEvent();
        }

        //执行其他线程提交的任务
        doPendingTasks();
    }
}

void EventLoop::quit()
{
    quit_.store(true);

    // 可能正阻塞在 epoll_wait，需要主动唤醒
    wakeup();
}