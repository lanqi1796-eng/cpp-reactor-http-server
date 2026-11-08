cmake_minimum_required(VERSION 3.16)
project(CppReactorHttpServer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(server
    TcpServer.cpp
    EventLoop.cpp
    Channel.cpp
    Connection.cpp
    HttpRequest.cpp
    HttpResponse.cpp
    StaticFileHandler.cpp
    wrap.cpp
)

target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(server PRIVATE Threads::Threads)
target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic)

# 可选：cmake -S . -B build-asan -DENABLE_SANITIZERS=ON
option(ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(server PRIVATE -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined)
    target_link_options(server PRIVATE -fsanitize=address,undefined)
endif()
