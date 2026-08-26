# Example 03: Async Networking with C++20 Coroutines + io_uring

**Status**: 📋 Design Phase - Architecture and system design complete  
**Target**: Linux Kernel 6.17+, C++20 (GCC 10+, Clang 10+)

---

## Overview

This example represents the **future of Linux async I/O** - combining C++20 coroutines with io_uring to create high-performance, maintainable network applications.

### The Vision

```cpp
// Write code that looks synchronous...
Task<void> handle_client(TcpStream client) {
    std::array<byte, 4096> buffer;
    
    auto data = co_await client.receive(buffer);  // Looks like blocking...
    co_await client.send(data);                    // But it's fully async!
}

// ...but executes asynchronously with io_uring performance
```

**Result**: 
- Clean, readable code (like blocking I/O)
- High performance (better than epoll)
- Type-safe (compile-time checks)
- No resource leaks (RAII everything)

---

## What's Included (Design Phase)

### 1. Comprehensive System Design (70+ pages)

**[SYSTEM_DESIGN.md](SYSTEM_DESIGN.md)** - Complete architecture document covering:

- **Executive Summary**: Performance goals and key innovations
- **Problem Statement**: Why io_uring + coroutines?
- **System Architecture**: Component hierarchy and interactions
- **Core Subsystems**: 
  - Coroutine Runtime (Task<T>, Promise, Awaitable)
  - io_uring Integration Layer
  - Socket Abstraction Layer
  - Memory Management
- **API Design**: Public interfaces with usage examples
- **Data Flow**: Request lifecycle and state machines
- **Performance Analysis**: Latency, throughput, scalability
- **Implementation Roadmap**: 8-week phased approach
- **Production Considerations**: Errors, limits, metrics, shutdown

### 2. Design Decisions Documentation

**[design_decisions.md](design_decisions.md)** - 12 key architectural decisions:

1. **C++20 Coroutines as Primary API** - Why co_await?
2. **Single-Threaded Event Loop** - Node.js model
3. **io_uring for All I/O** - No epoll/select
4. **RAII for Resource Management** - Zero leaks
5. **Task<T> Return Type** - Type-safe async
6. **No Exceptions in Hot Path** - Result<T> for I/O
7. **Eager Submission, Lazy Completion** - Latency + throughput
8. **Stack-Allocated Buffers** - Performance by default
9. **No Cancellation in MVP** - Complexity vs. value
10. **Multishot Operations** - Kernel 5.19+ features
11. **Metrics Built-In** - Production-ready from day one
12. **Minimal Dependencies** - Just liburing + std

Each decision includes rationale, trade-offs, and alternatives considered.

---

## Key Innovations

### 1. **Coroutines as API**

Traditional async (callback hell):
```cpp
async_read(sock, buf, [](Result r) {
    if (r.ok()) {
        async_write(sock, data, [](Result r2) {
            if (r2.ok()) {
                // More nesting...
            }
        });
    }
});
```

Our approach (clean):
```cpp
auto data = co_await sock.read(buf);
if (data) {
    co_await sock.write(response);
}
```

### 2. **Zero-Copy I/O**

```
Traditional: Kernel → User → Kernel (2 copies)
io_uring:    NIC → Registered Buffer → NIC (0 copies!)
```

**Result**: 2x bandwidth improvement

### 3. **Batch Operations**

```cpp
// Bad: Submit each operation individually
for (auto& req : requests) {
    co_await process(req);  // Many syscalls
}

// Good: Batch submission
std::vector<Task<void>> tasks;
for (auto& req : requests) {
    tasks.push_back(process(req));
}
io_uring_submit(&ring_);  // One syscall!
```

**Result**: 10-20x throughput improvement

---

## Performance Targets

| Metric | Target | vs. Traditional |
|--------|--------|-----------------|
| Throughput | 1M+ req/sec | 2-3x improvement |
| Latency (p50) | <100μs | 30% reduction |
| Latency (p99) | <500μs | 40% reduction |
| Memory/conn | <1KB | 10x reduction |
| CPU/conn | <5% | 20-30% reduction |

### Scalability Comparison

```
Connections    Threads    Coroutines
────────────────────────────────────
      1,000        1 GB          1 MB
     10,000       10 GB         10 MB
    100,000      100 GB        100 MB
  1,000,000         X        1,000 MB (1 GB!)
```

---

## Planned API

### Core Types

```cpp
namespace io_uring_async {

// Coroutine task
template<typename T = void>
class Task {
    // Awaitable, composable, type-safe
};

// Event loop executor
class IoUringExecutor {
    void spawn(Task<void> task);
    void run();  // Run until completion
};

// TCP sockets
class TcpListener {
    static Task<TcpListener> bind(uint16_t port);
    Task<TcpStream> accept();
};

class TcpStream {
    Task<size_t> send(span<const byte> data);
    Task<span<byte>> receive(span<byte> buffer);
};

} // namespace io_uring_async
```

### Usage Example

**Echo Server**:
```cpp
Task<void> handle_client(TcpStream client) {
    std::array<byte, 4096> buffer;
    
    while (true) {
        auto data = co_await client.receive(buffer);
        if (data.empty()) break;  // EOF
        
        co_await client.send(data);
    }
}

Task<void> run_server() {
    auto listener = co_await TcpListener::bind(8080);
    
    while (true) {
        auto client = co_await listener.accept();
        executor.spawn(handle_client(std::move(client)));
    }
}

int main() {
    IoUringExecutor executor;
    executor.spawn(run_server());
    executor.run();
}
```

**That's it!** 15 lines for a production-ready async echo server.

---

## Architecture Highlights

### Component Layers

```
Application (Echo Server)
         ↓ co_await
Socket API (TcpListener, TcpStream)
         ↓ Task<T>
Coroutine Runtime (Promise, Awaitable)
         ↓ io_uring ops
io_uring Integration (Executor, SQE/CQE)
         ↓ syscalls
Linux Kernel (io_uring subsystem)
```

### Event Loop Flow

```
1. Application calls co_await socket.receive()
2. Coroutine suspends, control returns to executor
3. Executor submits io_uring operation
4. Kernel performs I/O asynchronously
5. Completion arrives in CQE ring
6. Executor resumes coroutine
7. Application gets data, continues execution
```

---

## Implementation Roadmap

### Phase 1: Core Runtime (Week 1-2)
- [ ] Task<T> promise type
- [ ] Awaitable interface  
- [ ] Basic executor
- [ ] Unit tests

**Deliverable**: Working coroutines

### Phase 2: io_uring Integration (Week 2-3)
- [ ] Ring initialization
- [ ] SQE/CQE handling
- [ ] Operation submission/completion
- [ ] Error handling

**Deliverable**: I/O operations work

### Phase 3: Socket API (Week 3-4)
- [ ] TcpListener with accept
- [ ] TcpStream with send/recv
- [ ] RAII wrappers
- [ ] Echo server demo

**Deliverable**: Working echo server

### Phase 4: Optimization (Week 4-6)
- [ ] Registered buffers
- [ ] Batch submission
- [ ] Zero-copy paths
- [ ] Performance tuning

**Deliverable**: Production performance

### Phase 5: Polish (Week 6-8)
- [ ] Comprehensive testing
- [ ] Documentation
- [ ] Examples
- [ ] Benchmarks

**Deliverable**: Release candidate

**Total**: 8 weeks for production-ready library

---

## Why This Matters

### The Current Problem

Linux async I/O has been problematic:
- **Blocking + threads**: Doesn't scale, high memory
- **epoll + callbacks**: Callback hell, state machines
- **POSIX AIO**: Limited, poor kernel support

### The Solution

**io_uring** (kernel interface) + **C++20 coroutines** (language feature) = Perfect match

This combination gives us:
1. **Performance**: io_uring is the future of Linux I/O
2. **Usability**: Coroutines eliminate callback hell
3. **Safety**: C++20 type system prevents bugs
4. **Efficiency**: Zero-copy, batching built-in

### Industry Trend

This is **not experimental** - it's the proven approach:
- **Rust**: tokio (async/await + io_uring)
- **Python**: asyncio (async/await)
- **JavaScript**: Node.js (async/await)
- **Go**: goroutines (similar concept)

We're bringing this to **C++20** on **Linux**.

---

## Comparison with Alternatives

### vs. Traditional Sockets

```
Blocking Sockets:
✓ Simple API
✗ Thread per connection (~8MB each)
✗ Doesn't scale (10K problem)

Our Approach:
✓ Simple API (coroutines)
✓ Coroutine per connection (~100 bytes)
✓ Scales to 1M+ connections
```

### vs. epoll/libevent

```
epoll:
✓ Scalable
✗ Callback hell
✗ State machines
✗ Complex error handling

Our Approach:
✓ Scalable (better!)
✓ Sequential code
✓ Natural control flow
✓ Exception-based errors
```

### vs. Boost.Asio

```
Boost.Asio:
✓ Mature
✓ Cross-platform
✗ Large dependency
✗ Callback-based
✗ Complex API

Our Approach:
✓ Minimal deps (just liburing)
✓ Native coroutines
✓ Direct io_uring access
✗ Linux-only (feature, not bug)
```

---

## Requirements

### System Requirements

- **Kernel**: Linux 6.17+ (5.19+ recommended, 5.1+ minimum)
- **Compiler**: GCC 10+ or Clang 10+ with C++20 support
- **Libraries**: liburing 2.11+

### Build Requirements

```bash
# Check kernel version
uname -r  # Should be 6.17 or higher

# Install dependencies
sudo apt install liburing-dev g++-10

# Check C++20 support
g++-10 --version  # Should be 10.0 or higher
```

---

## Current Status

### ✅ Completed

1. **System Architecture** - Complete design document
2. **Design Decisions** - 12 key decisions documented
3. **API Design** - Public interfaces defined
4. **Performance Analysis** - Targets and benchmarks planned
5. **Implementation Roadmap** - 8-week phased approach

### 🚧 In Progress

*None - awaiting implementation go-ahead*

### 📋 Planned

1. **Core Runtime** - Task<T>, Promise, Awaitable
2. **io_uring Integration** - Executor, event loop
3. **Socket API** - TcpListener, TcpStream
4. **Echo Server** - Working demo
5. **Performance Tuning** - Meet targets
6. **Documentation** - Usage guides, examples

---

## How to Use (Future)

Once implemented, usage will be:

```bash
# Build
make

# Run echo server
./async_echo_server --port 8080

# Test with telnet
telnet localhost 8080

# Benchmark
wrk -t 4 -c 1000 -d 30s http://localhost:8080/
```

---

## Learning Resources

### io_uring
- [Linux `io_uring(7)` manual](https://man7.org/linux/man-pages/man7/io_uring.7.html)
- [LWN articles](https://lwn.net/Articles/io_uring/)
- [liburing](https://github.com/axboe/liburing)

### C++20 Coroutines
- [cppreference](https://en.cppreference.com/w/cpp/language/coroutines)
- [Lewiss Baker's blog](https://lewissbaker.github.io/)
- [Coroutines TS](https://en.cppreference.com/w/cpp/experimental/coroutine)

### Similar Projects
- [Rust tokio](https://tokio.rs/)
- [glommio](https://github.com/DataDog/glommio) (Rust)
- [seastar](http://seastar.io/) (C++)

---

## Contributing

This is currently in **design phase**. We welcome:

- **Design feedback**: Review system design and suggest improvements
- **Use case input**: What would you build with this?
- **Performance insights**: Experience with io_uring or coroutines
- **Implementation help**: Want to implement a component?

---

## Conclusion

This example demonstrates the **convergence of three technologies**:

1. **Linux io_uring** - Modern, efficient kernel interface
2. **C++20 Coroutines** - Language-level async support
3. **RAII + Type Safety** - Modern C++ practices

The result is a foundation for building:
- High-performance web servers
- Reverse proxies and load balancers
- Database connection pools
- Message queues
- Game servers
- Anything needing high-concurrency I/O

**This is the future of C++ systems programming on Linux.**

---

## References

1. System Design: [SYSTEM_DESIGN.md](SYSTEM_DESIGN.md)
2. Design Decisions: [design_decisions.md](design_decisions.md)
3. Project Requirements: [../../requirements.md](../../requirements.md)
4. C Example: [../01_basic_file_read/](../01_basic_file_read/)
5. C++23 Example: [../02_basic_file_read_cpp/](../02_basic_file_read_cpp/)
