//HttpResponse
#pragma once

#include <string>
#include <unordered_map>

class HttpResponse
{
public:
    HttpResponse();

    //设置状态码和状态信息
    void setStatus(int code,const std::string& message);

    //设置响应头
    bool setHeader(const std::string& key,
                   const std::string& value);
    
    //设置响应正文
    void setBody(const std::string& body);

    //生成完整HTTP响应字符串
    std::string toString() const;

private:
    // 保存原始 Header 名称和对应值
    struct HeaderField
    {
        std::string name;
        std::string value;
    };

    //将Header名称统一转成小写
    static std::string normalizeKey(const std::string& key);

private:
    int statuscode_;//状态码
    std::string statusMessage_;//状态行

    std::unordered_map<std::string,HeaderField> headers_;//消息头

    std::string body_;//响应正文
};