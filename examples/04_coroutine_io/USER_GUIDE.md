# io_uring_async User Guide

A modern C++20 coroutine-based library for high-performance asynchronous I/O using io_uring.

## Table of Contents

1. [Quick Start](#quick-start)
2. [Core Concepts](#core-concepts)
3. [API Reference](#api-reference)
4. [Examples](#examples)
5. [Best Practices](#best-practices)
6. [Performance Tips](#performance-tips)
7. [Troubleshooting](#troubleshooting)

## Quick Start

### Requirements

- Linux kernel 6.17+ (for latest io_uring features)
- GCC 11+ or Clang 14+ (C++20 coroutines support)
- liburing installed

### Installation

```bash
# Clone or copy the library
cp -r include/io_uring_async /usr/local/include/

# Build and install library
make release
sudo make install
```

### Hello World Echo Server

```cpp
#include <io_uring_async/io_uring_async.hpp>
#include <iostream>

using namespace io_uring_async;

Task<void> handle_client(TcpStream client) {
    std::array<std::byte, 1024> buffer;
    
    while (true) {
        auto recv_result = co_await client.receive(buffer);
        if (!recv_result || recv_result.value() == 0) break;
        
        size_t n = recv_result.value();
        co_await client.send(std::span{buffer.data(), n});
    }
}

Task<void> run_server(IoUringExecutor& executor) {
    auto listener = TcpListener::bind(executor, 8080).value();
    std::cout << "Listening on port 8080\n";
    
    while (true) {
        auto fd = (co_await listener.accept()).value();
        executor.spawn(handle_client(TcpStream{executor, fd}));
    }
}

int main() {
    auto executor = IoUringExecutor::create().value();
    executor.spawn(run_server(executor));
    executor.run();
}
```

## Core Concepts

### Result<T, E> - Type-Safe Error Handling

The library uses `Result<T, E>` instead of exceptions for predictable error handling:

```cpp
Result<int, Error> divide(int a, int b) {
    if (b == 0) {
        return Unexpected(Error::custom(-1, "Division by zero"));
    }
    return a / b;
}

// Usage
auto result = divide(10, 2);
if (result) {
    std::cout << "Result: " << result.value() << "\n";
} else {
    std::cerr << "Error: " << result.error().what() << "\n";
}

// Or use monadic operations
auto doubled = divide(10, 2)
    .map([](int x) { return x * 2; })
    .value_or(0);
```

### Task<T> - Lazy Coroutines

Tasks are lazy - they don't start executing until awaited:

```cpp
Task<int> compute() {
    co_return 42;
}

// This doesn't execute yet
auto task = compute();

// Execution starts here
int result = co_await task;
```

**Why lazy?** Enables:
- Optimal io_uring batching (submit multiple operations in one syscall)
- Conditional execution
- Structured concurrency

### IoUringExecutor - The Event Loop

The executor manages:
- Coroutine scheduling
- io_uring submission/completion
- Operation lifetime

```cpp
auto executor = IoUringExecutor::create(256).value();  // 256 entry queue

// Spawn tasks
executor.spawn(my_task());
executor.spawn(another_task());

// Run until all tasks complete
executor.run();
```

### Awaitables - Async Operations

All I/O operations return awaitables that integrate with io_uring:

```cpp
Task<void> example(TcpStream& stream) {
    std::array<std::byte, 1024> buffer;
    
    // These all return awaitables
    auto recv_result = co_await stream.receive(buffer);
    auto send_result = co_await stream.send(buffer);
    
    // Results are Result<T, Error>
    if (recv_result) {
        size_t bytes = recv_result.value();
    }
}
```

## API Reference

### Result<T, E>

```cpp
template<typename T, typename E>
class Result {
public:
    // Construction
    Result(T value);
    Result(Unexpected<E> error);
    
    // Query
    bool has_value() const noexcept;
    explicit operator bool() const noexcept;
    
    // Access (throws if wrong variant)
    T& value();
    const T& value() const;
    E& error();
    const E& error() const;
    
    // Safe access
    T value_or(T default_value) const;
    
    // Monadic operations
    template<typename F>
    auto map(F&& func) -> Result<U, E>;
    
    template<typename F>
    auto and_then(F&& func) -> Result<U, E>;
    
    template<typename F>
    auto or_else(F&& func) -> Result<T, E2>;
};
```

### Task<T>

```cpp
template<typename T = void>
class Task {
public:
    // Task is move-only
    Task(Task&&) noexcept;
    Task& operator=(Task&&) noexcept;
    
    // Awaitable interface
    bool await_ready() const noexcept;  // Always false (lazy)
    auto await_suspend(std::coroutine_handle<>);
    T await_resume();
    
    // Query
    bool valid() const noexcept;
    bool done() const noexcept;
};
```

### IoUringExecutor

```cpp
class IoUringExecutor {
public:
    // Factory
    static Result<IoUringExecutor, Error> create(
        unsigned queue_depth = 256
    );
    
    // Task management
    void spawn(Task<void>&& task);
    
    // Event loop
    Result<void, Error> run();  // Run until complete
    Result<bool, Error> run_once();  // Single iteration
    
    // Advanced
    IoUring& ring();
    uint64_t allocate_operation_id();
};
```

### TcpListener

```cpp
class TcpListener {
public:
    // Factory
    static Result<TcpListener, Error> bind(
        IoUringExecutor& executor,
        uint16_t port,
        int backlog = 128
    );
    
    // Accept connections
    AcceptAwaitable accept();
    
    // Query
    uint16_t port() const noexcept;
    int fd() const noexcept;
    
    // Cleanup
    void close() noexcept;
};
```

### TcpStream

```cpp
class TcpStream {
public:
    TcpStream(IoUringExecutor& executor, int fd);
    
    // I/O operations
    SendAwaitable send(std::span<const std::byte> buffer);
    RecvAwaitable receive(std::span<std::byte> buffer);
    
    // Shutdown
    Result<void, Error> shutdown_write();
    Result<void, Error> shutdown_read();
    Result<void, Error> shutdown();
    
    // Query
    int fd() const noexcept;
    bool is_open() const noexcept;
    
    // Cleanup
    void close() noexcept;
};
```

## Examples

### Echo Server with Error Handling

```cpp
Task<void> handle_client(TcpStream client, int id) {
    std::array<std::byte, 4096> buffer;
    
    while (true) {
        auto recv_result = co_await client.receive(buffer);
        
        if (!recv_result) {
            std::cerr << "Client " << id << " recv error: " 
                      << recv_result.error().what() << "\n";
            break;
        }
        
        size_t n = recv_result.value();
        if (n == 0) break;  // Clean shutdown
        
        auto send_result = co_await client.send(
            std::span{buffer.data(), n}
        );
        
        if (!send_result) {
            std::cerr << "Client " << id << " send error: " 
                      << send_result.error().what() << "\n";
            break;
        }
    }
}
```

### Chaining Operations

```cpp
Task<Result<std::string, Error>> read_request(TcpStream& stream) {
    std::array<std::byte, 1024> buffer;
    
    auto result = co_await stream.receive(buffer);
    
    co_return result.map([&buffer](size_t n) {
        return std::string(
            reinterpret_cast<char*>(buffer.data()), 
            n
        );
    });
}
```

### Multiple Concurrent Clients

```cpp
Task<void> run_server(IoUringExecutor& executor) {
    auto listener = TcpListener::bind(executor, 8080).value();
    int client_id = 0;
    
    while (true) {
        auto accept_result = co_await listener.accept();
        if (!accept_result) continue;
        
        TcpStream client{executor, accept_result.value()};
        
        // Spawn independent task for each client
        executor.spawn(handle_client(std::move(client), ++client_id));
    }
}
```

## Best Practices

### 1. Always Check Results

```cpp
// ❌ Bad - ignores errors
auto result = co_await stream.receive(buffer);
size_t n = result.value();  // Throws if error!

// ✅ Good - handles errors
auto result = co_await stream.receive(buffer);
if (!result) {
    handle_error(result.error());
    co_return;
}
size_t n = result.value();
```

### 2. Use RAII for Resources

```cpp
// ✅ Good - automatic cleanup
{
    auto listener = TcpListener::bind(executor, 8080).value();
    // ... use listener ...
}  // Automatically closed

// ❌ Bad - manual cleanup needed
auto listener = TcpListener::bind(executor, 8080).value();
listener.close();  // Easy to forget!
```

### 3. Proper Error Propagation

```cpp
Task<Result<void, Error>> process_request(TcpStream& stream) {
    auto recv_result = co_await stream.receive(buffer);
    if (!recv_result) {
        co_return Unexpected(recv_result.error());
    }
    
    // Process data...
    
    auto send_result = co_await stream.send(response);
    if (!send_result) {
        co_return Unexpected(send_result.error());
    }
    
    co_return {};
}
```

### 4. Structured Concurrency

```cpp
// Spawn related tasks together
Task<void> handle_connection(TcpStream stream) {
    // Both tasks work on same connection
    executor.spawn(read_loop(stream));
    executor.spawn(write_loop(stream));
}
```

## Performance Tips

### 1. Buffer Reuse

```cpp
// ✅ Reuse buffers
std::array<std::byte, 4096> buffer;
while (true) {
    auto n = (co_await stream.receive(buffer)).value();
    co_await stream.send(std::span{buffer.data(), n});
}

// ❌ Allocates every iteration
while (true) {
    std::vector<std::byte> buffer(4096);
    // ...
}
```

### 2. Appropriate Queue Depth

```cpp
// For low-concurrency workloads
auto executor = IoUringExecutor::create(64);

// For high-concurrency servers
auto executor = IoUringExecutor::create(256);

// For very high throughput
auto executor = IoUringExecutor::create(512);
```

### 3. Batch Operations

io_uring automatically batches operations. The lazy task evaluation helps:

```cpp
// These will be submitted together
auto task1 = read_file(fd1);
auto task2 = read_file(fd2);
auto task3 = read_file(fd3);

// All submitted in one syscall when awaited
auto [r1, r2, r3] = co_await std::tuple{task1, task2, task3};
```

## Troubleshooting

### Kernel Too Old

**Error**: `io_uring_queue_init: Function not implemented`

**Solution**: Upgrade to kernel 5.19+ (6.17+ recommended)

```bash
uname -r  # Check kernel version
```

### Queue Full

**Error**: `Failed to get SQE`

**Solution**: Increase queue depth or submit more frequently

```cpp
// Increase queue depth
auto executor = IoUringExecutor::create(512);
```

### Segfault on Co_await

**Cause**: Awaiting destroyed Task or invalid stream

**Solution**: Ensure proper lifetime management

```cpp
// ❌ Bad - stream destroyed before task completes
{
    TcpStream stream{executor, fd};
    executor.spawn(handle_client(std::move(stream)));
}  // stream destroyed!

// ✅ Good - move stream into task
TcpStream stream{executor, fd};
executor.spawn(handle_client(std::move(stream)));
```

### Resource Leaks

**Diagnosis**: Run with Valgrind

```bash
valgrind --leak-check=full ./your_program
```

**Common causes**:
- Not closing streams
- Circular Task references
- Forgetting to run executor

## Thread Safety

⚠️ **Single-threaded**: The current executor is single-threaded.

- All operations must be on the same thread
- Spawned tasks run on the executor's thread
- No synchronization needed within tasks

For multi-threaded applications, create one executor per thread.

## Limitations

Current version:
- Single-threaded executor only
- No timeout support yet
- No registered buffer support
- IPv4 only (IPv6 coming soon)

See [DESIGN.md](DESIGN.md) for planned features.

## Support

- Issues: File in GitHub repository
- Design rationale: See [design_decisions.md](design_decisions.md)
- Architecture: See [ARCHITECTURE_NOTES.md](ARCHITECTURE_NOTES.md)

## License

[Specify your license here]
