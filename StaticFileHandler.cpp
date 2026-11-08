#include "StaticFileHandler.h"

#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

StaticFileHandler::StaticFileHandler(const std::string& root)
{
    root_ = fs::weakly_canonical(root);
}

//得到文件后缀名
std::string StaticFileHandler::getContentType(const fs::path& path)
{
    std::string ext = path.extension().string();

    if(ext == ".html" || ext == ".htm")
        return "text/html; charset=utf-8";

    if(ext == ".css")
        return "text/css; charset=utf-8";

    if(ext == ".js")
        return "application/javascript; charset=utf-8";

    if(ext == ".json")
        return "application/json; charset=utf-8";

    if(ext == ".txt")
        return "text/plain; charset=utf-8";

    if(ext == ".png")
        return "image/png";

    if(ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";

    if(ext == ".gif")
        return "image/gif";

    if(ext == ".svg")
        return "image/svg+xml";

    if(ext == ".ico")
        return "image/x-icon";

    return "application/octet-stream";
}


//读文件
bool StaticFileHandler::readFile(const fs::path& path,std::string& data)
{
    std::ifstream file(
        path,
        std::ios::binary
    );

    if(!file.is_open())
    {
        return false;
    }

    std::ostringstream buffer;

    buffer << file.rdbuf();

    //读到data中
    data = buffer.str();

    return true;
}

//保证最终路径始终在path里面
bool StaticFileHandler::isInsideRoot(const fs::path& path) const
{
    auto rootIt = root_.begin();
    auto pathIt = path.begin();

    for(; rootIt != root_.end(); ++rootIt, ++pathIt)
    {
        if(pathIt == path.end())
        {
            return false;
        }

        if(*rootIt != *pathIt)
        {
            return false;
        }
    }

    return true;
}

//核心handle()
StaticFileHandler::Result StaticFileHandler::handle(const std::string& urlPath,
                                                    HttpResponse& response) const
{
    std::string path = urlPath;

    // 默认访问 index.html
    if(path == "/")
    {
        path = "/index.html";
    }

    // URL 应该以 / 开头
    if(path.empty() || path[0] != '/')
    {
        return Result::Forbidden;
    }

    // 去掉最前面的 /
    fs::path relative = fs::path(path.substr(1)).lexically_normal();


    // 不允许绝对路径
    if(relative.is_absolute())
    {
        return Result::Forbidden;
    }


    // 拒绝 ..
    for(const auto& part : relative)
    {
        if(part == "..")
        {
            return Result::Forbidden;
        }
    }


    fs::path filePath = fs::weakly_canonical(root_ / relative);


    // 防止逃出 www
    if(!isInsideRoot(filePath))
    {
        return Result::Forbidden;
    }


    // 文件不存在
    if(!fs::exists(filePath))
    {
        return Result::NotFound;
    }


    // 暂时只允许普通文件
    if(!fs::is_regular_file(filePath))
    {
        return Result::Forbidden;
    }


    std::string data;

    if(!readFile(filePath, data))
    {
        return Result::Error;
    }


    // 构造 200 OK
    response.setStatus(200, "OK");

    response.setHeader(
        "Content-Type",
        getContentType(filePath)
    );

    response.setBody(data);

    return Result::Ok;
}