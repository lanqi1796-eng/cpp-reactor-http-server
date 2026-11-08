#pragma once
#include <string>
#include <unordered_map>

class HttpRequest
{
public:
    enum class ParseResult
    {
        NeedMore,       // 数据还没有接收完整
        Complete,       // 请求解析完成
        BadRequest,     // 请求格式错误
        PayloadTooLarge,// Body 超出限制
        HeaderTooLarge, // Header 超出限制
        UnsupportedTransferEncoding
    };

    ParseResult parse(const std::string& message,
               size_t& consumed);
    //consumed:当前完整请求占用的字节

    void reset();   //解析新请求前先清理旧数据

    const std::string& getMethod() const;
    const std::string& getPath() const;
    const std::string& getVersion() const;

    bool getHeader(
        const std::string& key,
        std::string& value
    ) const;

    const std::string& getBody() const;

    //判断是否 Keep-Alive
    bool shouldKeepAlive() const;

private:
    std::string method_;
    std::string path_;
    std::string version_;

    std::string target_;    //保存完整请求目标
    std::string body_;

    std::unordered_map<std::string,std::string> headers_;
};