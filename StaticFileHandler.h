#pragma once

#include "HttpResponse.h"

#include <filesystem>
#include <string>

class StaticFileHandler
{
public:
    enum class Result
    {
        Ok,
        NotFound,
        Forbidden,
        Error
    };

    //禁止隐式转换 构造函数
    explicit StaticFileHandler(const std::string& root);

    Result handle(
        const std::string& urlPath,
        HttpResponse& response
    ) const;

private:
    static std::string getContentType(
        const std::filesystem::path& path
    );

    static bool readFile(
        const std::filesystem::path& path,
        std::string& data
    );

    bool isInsideRoot(
        const std::filesystem::path& path
    ) const;

private:
    std::filesystem::path root_;
};