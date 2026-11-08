#include "HttpRequest.h"

#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <charconv>

constexpr size_t MAX_HEADER_SIZE = 16*1024; //16KB
constexpr size_t MAX_BODY_SIZE = 1024*1024; //1MB

//更新
void HttpRequest::reset()
{
    method_.clear();
    path_.clear();
    version_.clear();

    target_.clear();
    body_.clear();

    headers_.clear();
}


HttpRequest::ParseResult 
HttpRequest::parse(const std::string& message,
                   size_t& consumed)
{
    consumed = 0;

    reset();

    //找HTTP Header的结束位置
    size_t headerEnd = message.find("\r\n\r\n");
    if(headerEnd == std::string::npos)
    {//还没找到 \r\n\r\n，说明 Header 还没结束
        if(message.size()>MAX_HEADER_SIZE)
        {//这时发现size过大了就意味着这个头文件一定过大了
            return ParseResult::HeaderTooLarge;
        }
        return ParseResult::NeedMore;
    }

    if(headerEnd+4>MAX_HEADER_SIZE)
    {
        return ParseResult::HeaderTooLarge;
    }

    //找到请求行
    size_t lineEnd = message.find("\r\n");
    if(lineEnd == std::string::npos)
    {
        return ParseResult::BadRequest;
    }

    //解析请求行----------------------------------------------
    std::string requestLine = message.substr(0, lineEnd);
    std::istringstream iss(requestLine);

    iss >> method_
        >> target_
        >> version_;

    if(method_.empty() || target_.empty() || version_.empty())
    {
        return ParseResult::BadRequest;
    }

    //设置文件路径
    size_t queryPos = target_.find('?');

    if(queryPos == std::string::npos)
    {
        path_ = target_;
    }
    else
    {
        path_ = target_.substr(0, queryPos);
    }


    //解析Header--------------------------------------------
    size_t current = lineEnd+2;

    while(current < headerEnd)
    {
        size_t nextLine = message.find("\r\n", current);

        if(nextLine == std::string::npos)
        {
            return ParseResult::BadRequest;
        }

        std::string line = message.substr(current,
                                          nextLine - current
                                         );

        size_t colon = line.find(':');

        if(colon == std::string::npos)
        {
            return ParseResult::BadRequest;
        }

        std::string key = line.substr(0, colon);

        //把所有的key都转成小写，方便对应
        std::transform(key.begin(),
                       key.end(),
                       key.begin(),
                       [](unsigned char c)
                       {
                            return std::tolower(c);
                       });

        std::string value = line.substr(colon + 1);

        // 去掉 value 前面的空格
        while(!value.empty() && (value.front() == ' ' || 
              value.front() == '\t') )
        {
            value.erase(0,1);
        }

        while(!value.empty() && (value.back() == ' ' ||
              value.back() == '\t'))
        {
            value.pop_back();
        }

        headers_[key] = value;

        current = nextLine + 2;
    }

    //读取Content-Length
    size_t bodyStart = headerEnd+4;
    size_t contentLength = 0;//两者加起来就是完整消息应该有的长度

    std::string contentLengthValue;
    std::string transferEncoding;

    //避免同时出现两种消息长度定义时产生歧义
    bool hasTE = getHeader("transfer-encoding", transferEncoding);
    bool hasCL = getHeader("content-length", contentLengthValue);
    if(hasTE && hasCL)
    {
        return ParseResult::BadRequest;
    }
    if(hasTE)
    {
        return ParseResult::UnsupportedTransferEncoding;
    }


    //正式开始读Content-Length
    //用C++17的from_chars
    if(hasCL)
    {
        auto result = std::from_chars(
            contentLengthValue.data(),
            contentLengthValue.data() + contentLengthValue.size(),
            contentLength
        );

        if(result.ec != std::errc{} ||
        result.ptr != contentLengthValue.data() + contentLengthValue.size())
        {
            return ParseResult::BadRequest;
        }
    }

    if(contentLength > MAX_BODY_SIZE)
    {
        return ParseResult::PayloadTooLarge;
    }

    //判断消息是否完整
    if(message.size()<bodyStart+contentLength)
    {
        return ParseResult::NeedMore;
    }

    //确认完整后将body完整的截出来
    body_ = message.substr(bodyStart,contentLength);

    //计算consumed,也就是完整的一条消息需要占多少
    consumed = bodyStart+contentLength;

    return ParseResult::Complete;
}


bool HttpRequest::getHeader( const std::string& key, std::string& value) const
{
    std::string lowerKey = key;

    std::transform(lowerKey.begin(),lowerKey.end(),lowerKey.begin(),[](unsigned char c)
                   {
                        return std::tolower(c);
                   });

    auto it = headers_.find(lowerKey);
    if(it == headers_.end())
    {
        return false;
    }

    value = it->second;
    return true;
}

const std::string& HttpRequest::getMethod() const
{
    return method_;
}

const std::string& HttpRequest::getPath() const
{
    return path_;
}

const std::string& HttpRequest::getVersion() const
{
    return version_;
}

const std::string& HttpRequest::getBody() const
{
    return body_;
}

bool HttpRequest::shouldKeepAlive() const
{
    std::string connection;

    //HTTP/1.1 默认长连接
    if(version_ == "HTTP/1.1")
    {
        //如果长连接中connection头没有内容
        if(!getHeader("connection",connection))
        {
            return true;//默认为长连接
        }

        //如果connection有内容，就将内容转为小写
        std::transform(connection.begin(),connection.end(),
                       connection.begin(),[](unsigned char c)
                       {
                            return std::tolower(c);
                       });
        
        //如果不是close就返回true,如果是close就返回false
        return connection != "close";
    }

    //HTTP/1.0 默认关闭
    if(version_ == "HTTP/1.0")
    {
        if(!getHeader("connection",connection))
        {
            return false;
        }

        std::transform(connection.begin(),connection.end(),
                       connection.begin(),[](unsigned char c)
                       {
                            return std::tolower(c);
                       });
        
        return connection != "keep-alive";
    }

    //如果都不是
    return false;
}
