# C++ Reactor HTTP Server

A learning-oriented HTTP/1.1 server implemented in **C++17** on Linux, using **epoll (level-triggered), non-blocking TCP sockets, a pthread worker pool, and eventfd-based cross-thread notifications**.

> **Status: v0.1.0 (learning project / MVP).** Functionality demonstrated locally; not production-hardened. No unsupported high-concurrency or QPS claims are made.

## Architecture

```text
Client TCP connections
        |
        v
EventLoop (epoll LT, single I/O thread)
   |       |          |
 Channel  Connection  eventfd <--- queueInLoop() <--- worker threads
              |                                      ^
      input/output buffers                           |
              |                                      |
      HttpRequest / HttpResponse ---------> ThreadPool (POST /echo)
              |
      StaticFileHandler (GET)
              |
             www/
```

**Thread ownership:** Worker threads execute business logic only. Socket I/O, Channel registration, and Connection output buffers are managed by the EventLoop thread. Completed business results return through `queueInLoop()` and `eventfd`; `weak_ptr<Connection>` avoids writing to a reused file descriptor after disconnection.

## Features

- HTTP/1.1 GET and POST (`POST /echo`)
- Static files: HTML, CSS, JS, and binary files
- Content-Length, basic error responses, keep-alive and close-after-write
- Non-blocking client sockets and partial-write handling using outputBuffer + EPOLLOUT
- Worker pool for asynchronous `/echo` processing; eventfd for returning results to EventLoop
- Connection lifetime managed with smart pointers

## Build

Requirements: Linux or WSL2, GCC with C++17 support, CMake, pthread.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/server
```

Run from the **project root directory**, because the current static-file root is `./www`.

## Smoke tests

In another terminal:

```bash
python3 tests/smoke_test.py
curl -v http://127.0.0.1:9999/
curl -v -d 'hello=world' http://127.0.0.1:9999/echo
```

Binary integrity (create temporary fixture locally, do not commit it):

```bash
dd if=/dev/urandom of=www/test32.bin bs=1M count=32
curl -fsS -o /tmp/test32.bin http://127.0.0.1:9999/test32.bin
cmp www/test32.bin /tmp/test32.bin
```

Sanitizer build:

```bash
cmake -S . -B build-asan -DENABLE_SANITIZERS=ON
cmake --build build-asan -j
./build-asan/server
```

## Current limits / roadmap

- Fixed worker pool; no dynamic scaling yet
- No idle timeout / timerfd (planned)
- Synchronous static-file disk reads; no sendfile/mmap yet
- Per-connection output-buffer backpressure and full shutdown sequencing need hardening
- No TLS, chunked request support, or production HTTP protocol compliance guarantee
- Benchmark results (QPS, tail latency, CPU/memory) not yet published

## Project layout

```text
TcpServer.cpp             Server entry and routing
EventLoop.{h,cpp}         epoll loop, eventfd, queued callbacks
Channel.{h,cpp}           fd event dispatch
Connection.{h,cpp}        TCP I/O, buffers, close-after-write
HttpRequest.{h,cpp}       HTTP parser
HttpResponse.{h,cpp}      HTTP serializer
StaticFileHandler.{h,cpp} Static asset handling
ThreadPool.h/.tpp         Worker-pool implementation
TaskQueue.h/.tpp          Task queue
wrap.{h,cpp}             Socket wrappers
www/                     Website assets
```

## Notes

The project is intentionally iterative. Release v0.1.0 focuses on understanding and validating the Reactor event loop, asynchronous task handoff, and safe connection ownership. See repository commits for later improvements.
