# GitHub v0.1.0 release checklist

- [ ] POST /echo response body matches request body byte-for-byte
- [ ] GET /, 404, Connection: close, keep-alive and pipelining work
- [ ] Slow POST (simulated worker delay) does not delay independent GET
- [ ] Client disconnect during asynchronous task does not crash server
- [ ] Debug+ASan+UBSan build passes smoke tests
- [ ] No generated random .bin files, binaries, secrets, or old duplicate source versions are staged
- [ ] `cmake -S . -B build && cmake --build build -j` works from a clean checkout
- [ ] README command lines are accurate for the current source layout
- [ ] No unverified concurrency-performance claims in README or resume
- [ ] Create GitHub public repository; push v0.1.0; verify `git clone` and rebuild

## Upload from WSL (after inspecting `git status`)

```bash
cd ~/projects/my_project_10_25/project_10_26
# If the folder is not already a Git repository:
git init -b main

git status --short
git add CMakeLists.txt .gitignore README.md PUBLISH_CHECKLIST.md tests 
git add TcpServer.cpp Connection.cpp Connection.h Channel.cpp Channel.h EventLoop.cpp EventLoop.h HttpRequest.cpp HttpRequest.h HttpResponse.cpp HttpResponse.h StaticFileHandler.cpp StaticFileHandler.h ThreadPool.h ThreadPool.tpp TaskQueue.h TaskQueue.tpp wrap.cpp wrap.h www/index.html www/style.css
# Add other legitimate static assets explicitly after reviewing them.
git diff --cached --stat
git diff --cached --check
git commit -m "feat: release v0.1 Reactor HTTP server"
# Create an EMPTY GitHub repository (no auto-created README), then:
git remote add origin https://github.com/YOUR_ACCOUNT/cpp-reactor-http-server.git
git push -u origin main
git tag -a v0.1.0 -m "First working release"
git push origin v0.1.0
```

Check `git remote -v` first if `origin` already exists; do not add it again.
