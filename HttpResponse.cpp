//HttpResponse.cpp
#include "HttpResponse.h"

#include <algorithm>
#include <cctype>

HttpResponse::HttpResponse():statuscode_(200),statusMessage_("OK"){}

std::string HttpResponse::normalizeKey(const std::string& key)
{
    std::string result = key;

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c)
        {
            return std::tolower(c);
        }
    );

    return result;
}

//设置状态码和状态信息
void HttpResponse::setStatus(int code,const std::string& message)
{
    if(code < 100 || code > 599)
        return;

    if(message.find_first_of("\r\n") != std::string::npos)
        return;

    statuscode_ = code;
    statusMessage_ = message;
}

//设置响应头
bool HttpResponse::setHeader(const std::string& key,
                const std::string& value)
{
    if(key.empty()) return false;

    //防止Header注入
    if(key.find_first_of("\r\n:\t") != std::string::npos ||
       value.find_first_of("\r\n") != std::string::npos)
    {
        return false;
    }

    // 这两个字段由协议层统一管理
    std::string lowerKey = normalizeKey(key);

    if(lowerKey == "content-length" ||
       lowerKey == "transfer-encoding")
    {
        return false;
    }

    headers_[lowerKey] = {key,value};

    return true;
}

//设置响应正文
void HttpResponse::setBody(const std::string& body)
{
    body_ = body;
}

//生成完整HTTP响应字符串
std::string HttpResponse::toString() const
{
    std::string response;

    //状态行
    response +=
        "HTTP/1.1 "
        + std::to_string(statuscode_)
        + " "
        + statusMessage_
        + "\r\n";

    //响应头
    for(const auto& header : headers_)
    {
        response +=
            header.second.name
            + ": "
            + header.second.value
            + "\r\n";
    }

    //Content-Length
    response +=
        "Content-Length: "
        + std::to_string(body_.size())//body_.size()计算字节数
        + "\r\n";

    //空行
    response += "\r\n";

    //body
    response += body_;

    return response;
}