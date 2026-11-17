//Tcp_server
#include "Connection.h"
#include "wrap.h"
#include "ThreadPool.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Channel.h"
#include "EventLoop.h"
#include "StaticFileHandler.h"

#include <unistd.h>
#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <unordered_map>

#include <fcntl.h>
#include <memory>

#define PORT 9999

//定义任务类型为线程池模板做准备
struct BusinessTask
{
    // 用于向 EventLoop 归还任务结果
    EventLoop* loop = nullptr;

    // 不直接持有 Connection 的所有权
    std::weak_ptr<Connection> weakConn;

    // 真正的业务数据
    std::string data;

    // 当前请求是否需要保持 TCP 连接
    bool keepAlive = true;
};

//任务回调函数
void processBusiness(void* arg)
{
    BusinessTask* task = static_cast<BusinessTask*>(arg);

    // 当前只做 /echo：返回请求中的原始 Body
    std::string result = task->data;

    // 以后可以在这里加入耗时计算、业务逻辑等

    HttpResponse response;

    response.setStatus(200, "OK");

    response.setHeader(
        "Content-Type",
        "text/plain; charset=utf-8"
    );

    response.setHeader(
        "Connection",
        task->keepAlive ? "keep-alive" : "close"
    );

    response.setBody(result);

    std::string rawResponse = response.toString();


    std::weak_ptr<Connection> weakConn =
        task->weakConn;

    bool keepAlive = task->keepAlive;

    EventLoop* loop = task->loop;


    loop->queueInLoop(
        [weakConn,
         keepAlive,
         rawResponse = std::move(rawResponse)]()
        {
            // 注意：这里已经回到 EventLoop 线程

            auto conn = weakConn.lock();

            if(!conn)
            {
                // 客户端连接已经销毁
                return;
            }

            conn->send(rawResponse);


            // 短连接：发完响应后关闭
            if(!keepAlive)
            {
                conn->shutdownAfterWrite();
                return;
            }


            // 长连接：恢复读取
            conn->resumeReading();

            // 处理先前已经保存在 inputBuffer_ 的请求
            conn->processBufferedInput();
        }
    );
}


//辅助错误响应函数
HttpResponse makeErrorResponse(HttpRequest::ParseResult result)
{
    HttpResponse response;

    response.setHeader("Content-Type","text/plain; charset=utf-8");
    response.setHeader("Connection","close");

    switch(result)
    {
        case HttpRequest::ParseResult::BadRequest:
            response.setStatus(400,"Bad Request");
            response.setBody("400 Bad Request");
            break;
        case HttpRequest::ParseResult::PayloadTooLarge:
            response.setStatus(413,"Payload Too Large");
            response.setBody("413 Payload Too Large");
            break;
        case HttpRequest::ParseResult::HeaderTooLarge:
            response.setStatus(431,"Request Header Fields Too Large");
            response.setBody("431 Request Header Fields Too Large");
            break;
        case HttpRequest::ParseResult::UnsupportedTransferEncoding:
            response.setStatus(501,"Not Implemented");
            response.setBody("501 Transfer-Encoding Not Supported");
            break;

        default:
            response.setStatus(400,"Bad Request");
            response.setBody("400 Bad Request");
    }

    return response;
}

//设置非阻塞
bool setNonBlocking(int fd)
{
    // 获取 fd 原来的文件状态标志
    int flags = fcntl(fd, F_GETFL, 0);

    //如果获取失败，返回error
    if(flags == -1)
    {
        perror("fcntl F_GETFL");
        return false;
    }

    //保留原来的文件状态标志，同时增加非阻塞属性
    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL");
        return false;
    }

    return true;
}

int main()
{
    int listenfd = Socket(AF_INET,SOCK_STREAM,0);
    int opt = 1;
    setsockopt(listenfd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    //服务端地址
    sockaddr_in servaddr{};

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    
    //监听套接字绑定服务端地址
    Bind(listenfd,
         (struct sockaddr*)&servaddr,
         sizeof(servaddr));//注意类型

    //开始监听
    Listen(listenfd,128);


    //创建EventLoop
    EventLoop el;

    //创建ThreadPool
    ThreadPool<BusinessTask> tp(3, 8);

    //保存后续conn指针
    std::unordered_map<int, std::shared_ptr<Connection>> connections;


    //listenfd对应的Channel
    Channel listenChannel(listenfd);
    listenChannel.setEvents(EPOLLIN);

    
    StaticFileHandler staticFiles("./www");
    //新连接回调
    listenChannel.setReadCallback([&]()
    {
        sockaddr_in clieaddr;
        socklen_t clie_addr_len = sizeof(clieaddr);

        //连接客户端套接字
        int cfd = Accept(listenfd,
                         (struct sockaddr*)&clieaddr,
                         &clie_addr_len);//注意类型
                
        //新接收的客户端socket设置非阻塞
        if(!setNonBlocking(cfd))
        {
            Close(cfd);
            return;
        }

        //将连接的cfd接入Connection
        auto  conn = std::make_shared<Connection>(&el,cfd);
        connections[cfd] = conn;

        std::cout<<"new client fd = "
                 <<cfd
                 <<std::endl<<std::endl;


        conn->setMessageCallback(
            [&](Connection* conn,const std::string& message)
            {
                //当前已经处理的字节数
                size_t offset = 0;

                HttpRequest request;
                size_t consumed = 0;
                while(offset<message.size())
                {
                    std::string remaining = message.substr(offset);
                    auto result = request.parse(remaining,consumed);

                    //请求不完整
                    if(result == HttpRequest::ParseResult::NeedMore) break;

                    //先处理合法请求
                    if(result != HttpRequest::ParseResult::Complete){
                        HttpResponse response = makeErrorResponse(result);

                        std::string raw = response.toString();

                        conn->send(raw);
                        std::cout<<"HTTP request parse error"<<std::endl;

                        conn->shutdownAfterWrite();
                        return;
                    }

                    if(consumed == 0 || consumed>remaining.size()) 
                    {
                        conn->shutdownAfterWrite();
                        return;
                    }

                    if(request.getMethod() == "POST" &&
                    request.getPath() == "/echo")
                    {
                        //创建任务数据
                        auto data = std::make_unique<BusinessTask>();

                        data->loop = &el;

                        // 仅保存弱引用，不保存 fd
                        data->weakConn = conn->shared_from_this();

                        // 拷贝请求 Body
                        data->data = request.getBody();

                        // 保存这次请求的 Keep-Alive 语义
                        data->keepAlive = request.shouldKeepAlive();


                        //提交任务到线程池
                        bool accepted = false;

                        try
                        {
                            accepted = tp.addTask(
                                Task<BusinessTask>(
                                    processBusiness,
                                    data.get()
                                )
                            );
                        }
                        catch(const std::exception& e)
                        {
                            std::cerr
                                << "ThreadPool addTask error: "
                                << e.what()
                                << std::endl;
                        }


                        //如果线程池不接受任务
                        if(!accepted)
                        {
                            HttpResponse response;

                            response.setStatus(
                                503,
                                "Service Unavailable"
                            );

                            response.setHeader(
                                "Content-Type",
                                "text/plain; charset=utf-8"
                            );

                            response.setHeader(
                                "Connection",
                                "close"
                            );

                            response.setBody(
                                "503 Service Unavailable"
                            );

                            conn->send(response.toString());
                            conn->shutdownAfterWrite();

                            return;
                        }


                        //提交成功，线程池接管任务参数
                        data.release();


                        //当前请求已经解析完整
                        offset += consumed;

                        //删除已处理的请求，保留后续请求
                        conn->consumeInput(offset);


                        //暂停当前 Connection 的读事件
                        conn->pauseReading();

                        // 当前请求的响应由 worker 完成后提交
                        // 不能继续解析同一连接上的下一条请求
                        return;
                    }

                    //业务处理------------------------------------------------------

                    HttpResponse response;
                    std::string body;
                    bool keepAlive;

                    //路由分发
                    if(request.getMethod() == "GET")
                    {
                        auto fileResult = staticFiles.handle( request.getPath(),
                                                              response );

                        if(fileResult == StaticFileHandler::Result::NotFound)
                        {
                            response.setStatus(
                                404,
                                "Not Found"
                            );

                            response.setHeader(
                                "Content-Type",
                                "text/plain; charset=utf-8"
                            );

                            response.setBody(
                                "404 Not Found"
                            );
                        }

                        else if(fileResult == StaticFileHandler::Result::Forbidden)
                        {
                            response.setStatus(
                                403,
                                "Forbidden"
                            );

                            response.setHeader(
                                "Content-Type",
                                "text/plain; charset=utf-8"
                            );

                            response.setBody(
                                "403 Forbidden"
                            );
                        }

                        else if(fileResult == StaticFileHandler::Result::Error)
                        {
                            response.setStatus(
                                500,
                                "Internal Server Error"
                            );

                            response.setHeader(
                                "Content-Type",
                                "text/plain; charset=utf-8"
                            );

                            response.setBody(
                                "500 Internal Server Error"
                            );
                        }


                        keepAlive = request.shouldKeepAlive();
                        response.setHeader("Connection",
                                        keepAlive? "keep-alive":"close");

                    }
                    else if(request.getMethod() != "GET" && request.getMethod() != "POST")
                    {
                        response.setStatus(405, "Method Not Allowed");
                        response.setHeader("Allow","GET, POST");
                        response.setHeader("Content-Type","text/plain; charset=utf-8");

                        body = "405 Method Not Allowed";

                        keepAlive = request.shouldKeepAlive();
                        response.setHeader("Connection",
                                        keepAlive? "keep-alive":"close");

                        //生成HTTP Response
                        response.setBody(body);
                    }
                    else
                    {
                        response.setStatus(404, "Not Found");
                        response.setHeader("Content-Type","text/plain; charset=utf-8");

                        body = "404 Not Found";

                        keepAlive = request.shouldKeepAlive();
                        response.setHeader("Connection",
                                        keepAlive? "keep-alive":"close");

                        //生成HTTP Response
                        response.setBody(body);
                    }

                    
                    std::string rawResponse = response.toString();

                    //发给客户端
                    conn->send(rawResponse);

                    //处理完毕之后
                    offset += consumed;

                    if(!keepAlive){
                        conn->shutdownAfterWrite();
                        return;
                    }
                }

                if(offset>0)
                {
                    conn->consumeInput(offset);
                }
            }
        );

        conn->setCloseCallback(
            [&connections](Connection* connection)
            {
                int fd = connection->getFd();
                auto it = connections.find(fd);

                if(it!=connections.end() && it->second.get()==connection)
                {
                    connections.erase(it);
                }
            }
        );

        conn->establish();
    });


    //listenfd 加入 EventLoop
    el.addChannel(&listenChannel);
    std::cout<<"server listening on port "<<PORT<<std::endl;

    el.loop();
    return 0;
}