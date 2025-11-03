# Architecture Notes: Implementation Guidance

**Project**: C++20 Coroutine io_uring Library  
**Document Type**: Technical Implementation Guide  
**Author**: Senior Systems Engineer  
**Date**: 2025-10-30  
**Audience**: Developers implementing the system

---

## 1. Overview

This document provides technical guidance for implementing the coroutine-based io_uring library described in `DESIGN.md`. It bridges the gap between architectural decisions and actual code, providing patterns, approaches, and implementation strategies.

**Document Purpose**:
- Technical details for developers
- Implementation patterns and approaches
- Data structure design guidance
- Algorithm outlines
- Integration strategies

**Not Covered Here**:
- High-level architectural decisions (see `DESIGN.md`)
- Complete API documentation (see `API_SPEC.md`)
- Actual implementation code (in source files)

---

## 2. Module Organization

### 2.1 Recommended File Structure

```
examples/04_coroutine_io/
├── include/
│   └── io_uring_async/
│       ├── core/
│       │   ├── result.hpp          # Result<T, E> type
│       │   ├── task.hpp            # Task<T> coroutine type
│       │   └── concepts.hpp        # C++20 concepts
│       ├── runtime/
│       │   ├── executor.hpp        # IoUringExecutor
│       │   ├── operation.hpp       # Operation base
│       │   └── awaitable.hpp       # Awaitable implementations
│       ├── io/
│       │   ├── tcp_listener.hpp    # TCP accept
│       │   ├── tcp_stream.hpp      # TCP send/recv
│       │   ├── udp_socket.hpp      # UDP operations
│       │   └── file.hpp            # Async file I/O
│       └── io_uring_async.hpp      # Convenience header
├── src/
│   ├── runtime/
│   │   └── executor.cpp            # Executor implementation
│   └── io/
│       ├── tcp_listener.cpp
│       └── tcp_stream.cpp
├── examples/
│   ├── echo_server.cpp
│   ├── http_server.cpp
│   └── file_copy.cpp
├── tests/
│   ├── unit/
│   │   ├── test_task.cpp
│   │   ├── test_result.cpp
│   │   └── test_executor.cpp
│   └── integration/
│       └── test_echo_server.cpp
├── CMakeLists.txt
├── Makefile
├── DESIGN.md
├── ARCHITECTURE_NOTES.md
├── API_SPEC.md
├── README.md
└── design_decisions.md
```

### 2.2 Module Dependencies

```
Application Code
    ↓ depends on
io/* (TcpListener, TcpStream, etc.)
    ↓ depends on
runtime/* (Executor, Operation, Awaitable)
    ↓ depends on
core/* (Task, Result, Concepts)
    ↓ depends on
C++20 Standard Library + liburing
```

**Key Principle**: No circular dependencies. Each layer only depends on layers below it.

---

## 3. Core Components Implementation

### 3.1 Result<T, E> Type

**Purpose**: Explicit error handling without exceptions

**Key Requirements**:
- Must be `[[nodiscard]]` to prevent ignoring errors
- Support both value and error states
- Provide monadic operations (map, and_then, or_else)
- Trivially copyable when T and E are trivial
- No dynamic allocation

**Implementation Approach**:

```cpp
template<typename T, typename E>
class Result {
    // Use std::variant or union storage
    // Track which alternative is active
    // Provide value() and error() accessors
    // Support pattern matching with visit()
};

// Key methods to implement:
// - has_value() const -> bool
// - value() & -> T&
// - value() && -> T&&
// - error() & -> E&
// - value_or(T default) -> T
// - map<U>(F func) -> Result<U, E>
// - and_then<U>(F func) -> Result<U, E>
// - or_else(F func) -> Result<T, E>
```

**Design Considerations**:
1. Use `union` with placement new/destroy for efficiency
2. Mark all accessors `[[nodiscard]]`
3. Provide both lvalue and rvalue overloads
4. Consider small-size optimization for common error types

### 3.2 Task<T> - The Coroutine Type

**Purpose**: Represents an async computation that can be awaited

**Key Requirements**:
- Must satisfy C++20 coroutine requirements
- Support composition via co_await
- Lazy evaluation (doesn't start until awaited)
- Move-only semantics
- RAII cleanup of coroutine frame

**Critical Components**:

**Promise Type**:
```cpp
template<typename T>
struct Task<T>::promise_type {
    // What gets returned from coroutine
    Task<T> get_return_object();
    
    // Suspend at start (lazy evaluation)
    std::suspend_always initial_suspend() noexcept;
    
    // Suspend at end (caller manages cleanup)
    std::suspend_always final_suspend() noexcept;
    
    // Handle co_return value
    void return_value(T value);
    
    // Handle exceptions
    void unhandled_exception();
    
    // Storage for result or exception
    std::variant<std::monostate, T, std::exception_ptr> result_;
    
    // Parent coroutine handle
    std::coroutine_handle<> continuation_;
};
```

**Awaitable Interface**:
```cpp
template<typename T>
class Task {
    // Check if already complete
    bool await_ready() const noexcept {
        return false; // Always suspend (lazy)
    }
    
    // What to do when suspending
    void await_suspend(std::coroutine_handle<> awaiter) {
        // Store continuation
        promise().continuation_ = awaiter;
        // Resume this task's coroutine
        coro_.resume();
    }
    
    // Return value when resuming
    T await_resume() {
        // Extract and return result
        // Or rethrow exception if present
    }
};
```

**Lifetime Management**:
- Coroutine frame created when function called
- Frame suspended at `initial_suspend()`
- Frame started when awaited (`await_suspend`)
- Frame destroyed when Task destroyed
- Must handle both normal completion and exceptions

**Special Case: Task<void>**:
```cpp
// Specialize for void return type
template<>
struct Task<void>::promise_type {
    void return_void(); // Instead of return_value
    // Rest similar
};
```

### 3.3 IoUringExecutor - The Event Loop

**Purpose**: Manage io_uring instance and schedule coroutines

**Key Responsibilities**:
1. Initialize and manage io_uring instance
2. Submit operations to submission queue
3. Poll completion queue
4. Resume coroutines on completion
5. Run ready-to-execute tasks

**Core Data Structures**:

```cpp
class IoUringExecutor {
private:
    // io_uring instance
    io_uring ring_;
    
    // Next operation ID (for tracking)
    uint64_t next_op_id_ = 0;
    
    // Map operation ID -> coroutine handle
    std::unordered_map<uint64_t, std::coroutine_handle<>> pending_ops_;
    
    // Queue of ready-to-run tasks
    std::deque<Task<void>> ready_queue_;
    
    // Batch size for submission
    static constexpr size_t BATCH_SIZE = 128;
    size_t pending_submissions_ = 0;
};
```

**Event Loop Algorithm**:

```
while (has_work_to_do) {
    // Phase 1: Run all ready tasks
    while (ready_queue not empty) {
        task = ready_queue.pop_front()
        task.resume()
    }
    
    // Phase 2: Submit pending operations
    if (pending_submissions > 0) {
        io_uring_submit(&ring)
        pending_submissions = 0
    }
    
    // Phase 3: Process completions
    while (cqe = io_uring_peek_cqe(&ring)) {
        op_id = cqe->user_data
        result = cqe->res
        
        handle = pending_ops[op_id]
        pending_ops.erase(op_id)
        
        // Store result in coroutine promise
        store_result(handle, result)
        
        // Resume coroutine
        ready_queue.push_back(handle)
        
        io_uring_cqe_seen(&ring, cqe)
    }
    
    // Phase 4: Wait if no work
    if (ready_queue empty && pending_ops not empty) {
        io_uring_wait_cqe(&ring, &cqe)
        // Process this cqe
    }
}
```

**Batching Strategy**:
- Accumulate SQEs in submission queue
- Submit when batch size reached OR no more work
- Reduces syscall overhead significantly

**Operation Submission Pattern**:

```cpp
template<typename Op>
auto IoUringExecutor::submit(Op&& operation) -> Task<typename Op::result_type> {
    // Get SQE
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    
    // Assign operation ID
    uint64_t op_id = next_op_id_++;
    
    // Prepare operation
    operation.prepare(sqe);
    io_uring_sqe_set_data(sqe, op_id);
    
    // Create awaitable
    co_return co_await IoUringAwaitable<typename Op::result_type>{
        .executor = this,
        .operation_id = op_id
    };
}
```

---

## 4. I/O Component Implementation

### 4.1 TcpListener Implementation Strategy

**Responsibilities**:
- Create and bind listening socket
- Accept incoming connections
- Return TcpStream for each connection

**Key Implementation Details**:

**Socket Setup**:
```cpp
// Create non-blocking socket
int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

// Set socket options
int opt = 1;
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

// Bind and listen
sockaddr_in addr = /* configure */;
bind(fd, (sockaddr*)&addr, sizeof(addr));
listen(fd, SOMAXCONN);
```

**Accept Operation**:
```cpp
Task<Result<TcpStream, Error>> TcpListener::accept() {
    // Prepare accept operation
    sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Submit to io_uring
    auto result = co_await executor_->submit(AcceptOperation{
        .fd = fd_,
        .addr = &client_addr,
        .addr_len = &addr_len
    });
    
    if (result < 0) {
        co_return Error{-result, "accept failed"};
    }
    
    int client_fd = result;
    co_return TcpStream{client_fd, executor_};
}
```

**Multishot Accept** (Optimization):
- Use `IORING_ACCEPT_MULTISHOT` flag
- Single SQE produces multiple CQEs
- Significant performance improvement
- Requires kernel 5.19+

### 4.2 TcpStream Implementation Strategy

**Responsibilities**:
- Send data asynchronously
- Receive data asynchronously
- Manage connection lifecycle

**Send Operation**:
```cpp
Task<Result<size_t, Error>> TcpStream::send(std::span<const std::byte> data) {
    // Prepare send operation
    auto result = co_await executor_->submit(SendOperation{
        .fd = fd_,
        .buffer = data.data(),
        .length = data.size(),
        .flags = 0
    });
    
    if (result < 0) {
        co_return Error{-result, "send failed"};
    }
    
    co_return static_cast<size_t>(result);
}
```

**Receive Operation**:
```cpp
Task<Result<size_t, Error>> TcpStream::receive(std::span<std::byte> buffer) {
    auto result = co_await executor_->submit(RecvOperation{
        .fd = fd_,
        .buffer = buffer.data(),
        .length = buffer.size(),
        .flags = 0
    });
    
    if (result < 0) {
        co_return Error{-result, "receive failed"};
    }
    
    co_return static_cast<size_t>(result);
}
```

**Resource Management**:
- File descriptor owned by TcpStream
- RAII cleanup in destructor
- Move-only semantics
- Graceful shutdown support

---

## 5. Memory Management Strategies

### 5.1 Buffer Ownership Models

**Model 1: User-Owned Buffers (Simplest)**
```cpp
// User allocates
std::array<std::byte, 4096> buffer;

// Library borrows (via span)
auto n = co_await stream.receive(std::span{buffer});

// User owns lifetime
```

**Advantages**:
- Zero overhead
- Clear ownership
- Works with stack allocation

**Disadvantages**:
- User must manage size
- No zero-copy optimization

**Model 2: Registered Buffers (Highest Performance)**
```cpp
// Register buffers with kernel
std::vector<std::array<std::byte, 4096>> buffer_pool(1024);
executor.register_buffers(buffer_pool);

// Use buffer by ID
auto n = co_await stream.send_registered(buffer_id);
```

**Advantages**:
- True zero-copy
- Kernel can DMA directly
- Maximum performance

**Disadvantages**:
- Setup cost
- Limited number
- Must manage IDs

**Model 3: Provided Buffers (Most Flexible)**
```cpp
// Executor provides buffer pool to kernel
executor.provide_buffers(buffer_group_id, buffer_pool);

// Kernel picks buffer
auto [data, buffer_id] = co_await stream.receive_provided();

// Return buffer when done
executor.return_buffer(buffer_group_id, buffer_id);
```

**Advantages**:
- Kernel manages allocation
- Good for variable sizes
- Clean API

**Disadvantages**:
- Kernel 5.19+ only
- More complex setup
- Must return buffers

### 5.2 Coroutine Frame Allocation

**Challenge**: Coroutine frames are typically heap-allocated

**Solutions**:

1. **Let Compiler Optimize** (Preferred):
   ```cpp
   // Compiler can often elide allocation
   Task<int> foo() {
       co_return 42;
   }
   
   int main() {
       int x = foo(); // May not allocate
   }
   ```

2. **Custom Allocator**:
   ```cpp
   struct Task<T>::promise_type {
       void* operator new(std::size_t size) {
           // Use custom allocator
           // E.g., pool allocator, arena
       }
       
       void operator delete(void* ptr) {
           // Return to pool
       }
   };
   ```

3. **Stateless Coroutines** (Future):
   ```cpp
   // P2300 sender/receiver model
   // May avoid some allocations
   ```

---

## 6. Error Handling Patterns

### 6.1 Error Categories

**System Errors** (errno-based):
```cpp
struct SystemError {
    int code;           // errno value
    std::string_view operation;
    
    std::string message() const {
        return std::format("{}: {}", operation, strerror(code));
    }
};
```

**io_uring Errors** (negative result codes):
```cpp
struct IoUringError {
    int code;           // Negative result
    std::string_view operation;
    
    std::string message() const {
        return std::format("{}: {}", operation, strerror(-code));
    }
};
```

**Application Errors** (domain-specific):
```cpp
struct ParseError {
    std::string_view input;
    size_t position;
    std::string reason;
};
```

### 6.2 Error Propagation Patterns

**Pattern 1: Early Return**:
```cpp
Task<Result<void, Error>> process() {
    auto data = co_await read();
    if (!data) {
        co_return std::unexpected(data.error());
    }
    
    auto result = co_await write(*data);
    if (!result) {
        co_return std::unexpected(result.error());
    }
    
    co_return {};
}
```

**Pattern 2: Monadic Chaining**:
```cpp
Task<Result<void, Error>> process() {
    co_return co_await read()
        .and_then([](auto data) { return write(data); })
        .and_then([](auto) { return flush(); });
}
```

**Pattern 3: Exception Boundaries**:
```cpp
Task<void> process() {
    try {
        auto data = co_await read().value(); // Throws on error
        co_await write(data).value();
    } catch (const Error& e) {
        log_error(e);
        throw;
    }
}
```

### 6.3 Timeout Handling

**Pattern**: Use io_uring timeout linked to operation

```cpp
Task<Result<size_t, Error>> TcpStream::receive_with_timeout(
    std::span<std::byte> buffer,
    std::chrono::milliseconds timeout)
{
    // Submit receive operation
    uint64_t recv_id = co_await executor_->submit_operation(
        RecvOperation{fd_, buffer}
    );
    
    // Submit linked timeout
    uint64_t timeout_id = co_await executor_->submit_timeout(
        timeout, recv_id
    );
    
    // Wait for whichever completes first
    auto result = co_await executor_->wait_either(recv_id, timeout_id);
    
    if (result.which == timeout_id) {
        co_return Error{ETIMEDOUT, "receive timed out"};
    }
    
    co_return result.value;
}
```

---

## 7. Performance Optimization Techniques

### 7.1 Submission Batching

**Problem**: Each io_uring_submit() is a syscall

**Solution**: Accumulate operations before submitting

```cpp
class IoUringExecutor {
    static constexpr size_t BATCH_SIZE = 128;
    size_t pending_count_ = 0;
    
    void maybe_submit() {
        if (pending_count_ >= BATCH_SIZE) {
            io_uring_submit(&ring_);
            pending_count_ = 0;
        }
    }
    
    void force_submit() {
        if (pending_count_ > 0) {
            io_uring_submit(&ring_);
            pending_count_ = 0;
        }
    }
};
```

**Impact**: 10-20x reduction in syscalls

### 7.2 Completion Polling

**Problem**: io_uring_wait_cqe() blocks

**Solution**: Use io_uring_peek_cqe() in a loop

```cpp
void process_completions() {
    io_uring_cqe* cqe;
    unsigned processed = 0;
    
    // Process all available completions
    while (io_uring_peek_cqe(&ring_, &cqe) == 0) {
        handle_completion(cqe);
        io_uring_cqe_seen(&ring_, cqe);
        ++processed;
    }
    
    // Only wait if nothing to process
    if (processed == 0 && has_pending_operations()) {
        io_uring_wait_cqe(&ring_, &cqe);
        handle_completion(cqe);
        io_uring_cqe_seen(&ring_, cqe);
    }
}
```

### 7.3 Zero-Copy Techniques

**Registered Buffers**:
```cpp
// One-time setup
std::vector<iovec> buffers;
for (auto& buf : buffer_pool) {
    buffers.push_back({buf.data(), buf.size()});
}
io_uring_register_buffers(&ring_, buffers.data(), buffers.size());

// Use in operations
io_uring_prep_read_fixed(sqe, fd, buffer, len, offset, buffer_index);
```

**Splice Operations**:
```cpp
// Zero-copy pipe between file descriptors
io_uring_prep_splice(sqe, fd_in, off_in, fd_out, off_out, len, flags);
```

**Direct Descriptors**:
```cpp
// Register file descriptors for faster operations
io_uring_register_files(&ring_, fds, num_fds);
io_uring_prep_read(sqe, fd_index | IOSQE_FIXED_FILE, ...);
```

### 7.4 CPU Cache Optimization

**Locality**: Keep hot data together
```cpp
struct Operation {
    uint64_t id;                    // 8 bytes
    std::coroutine_handle<> handle; // 8 bytes
    int result;                     // 4 bytes
    // Total: 20 bytes (fits in cache line)
};
```

**Alignment**: Align frequently accessed data
```cpp
struct alignas(64) HotData {
    // Aligns to cache line boundary
};
```

---

## 8. Testing Strategies

### 8.1 Unit Testing Approach

**Test Task<T> Lifecycle**:
```cpp
TEST(Task, BasicLifecycle) {
    auto task = make_simple_task();
    ASSERT_FALSE(task.done());
    
    task.resume();
    ASSERT_TRUE(task.done());
    
    auto result = task.get();
    ASSERT_EQ(result, expected_value);
}
```

**Test Error Propagation**:
```cpp
TEST(Result, ErrorPropagation) {
    Result<int, Error> r = Error{EINVAL, "test"};
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error().code, EINVAL);
}
```

### 8.2 Integration Testing

**Test Echo Server**:
```cpp
TEST(Integration, EchoServer) {
    IoUringExecutor executor;
    
    // Start server
    executor.spawn(run_echo_server(8080));
    
    // Connect client
    auto client = connect_tcp("localhost", 8080);
    
    // Send data
    client.send("Hello");
    
    // Verify echo
    auto received = client.receive();
    ASSERT_EQ(received, "Hello");
}
```

### 8.3 Stress Testing

**Many Connections**:
```cpp
TEST(Stress, ManyConnections) {
    constexpr int NUM_CLIENTS = 10000;
    
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        executor.spawn(client_task(i));
    }
    
    executor.run();
    
    ASSERT_EQ(completed_count, NUM_CLIENTS);
}
```

---

## 9. Debugging Support

### 9.1 Debug Mode Features

**Coroutine Tracking**:
```cpp
#ifdef DEBUG_MODE
class Task {
    std::source_location location_;
    std::string debug_name_;
    
    void set_debug_info(std::string name, std::source_location loc) {
        debug_name_ = name;
        location_ = loc;
    }
};
#endif
```

**Operation Logging**:
```cpp
void IoUringExecutor::submit_operation(Operation op) {
    #ifdef DEBUG_MODE
    log_debug("Submitting {} at {}:{}", 
              op.type, op.file, op.line);
    #endif
    
    // Normal submission
}
```

### 9.2 Tracing Support

**Timeline Tracing**:
```cpp
struct TraceEvent {
    uint64_t timestamp;
    std::string_view event_type;
    uint64_t operation_id;
    int result;
};

std::vector<TraceEvent> trace_buffer;

void record_event(auto... args) {
    if (tracing_enabled) {
        trace_buffer.emplace_back(args...);
    }
}
```

---

## 10. Build System Considerations

### 10.1 CMake Configuration

**Minimum version and requirements**:
```cmake
cmake_minimum_required(VERSION 3.20)
project(io_uring_async CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find liburing
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED liburing)
```

**Compiler flags**:
```cmake
# Performance
add_compile_options(-O3 -march=native)

# Warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Coroutine support
add_compile_options(-fcoroutines)

# Debug
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -fsanitize=address)
endif()
```

### 10.2 Makefile Alternative

**Simple Makefile for development**:
```makefile
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -fcoroutines
LDFLAGS = -luring

SRCS = $(wildcard src/**/*.cpp)
OBJS = $(SRCS:.cpp=.o)

all: libio_uring_async.a

libio_uring_async.a: $(OBJS)
	ar rcs $@ $^

clean:
	rm -f $(OBJS) libio_uring_async.a
```

---

## 11. Migration and Integration

### 11.1 Integrating with Existing Code

**Pattern**: Wrap synchronous code in coroutines

```cpp
// Existing synchronous code
int process_data(const Data& data);

// Wrapped version
Task<int> async_process_data(const Data& data) {
    // Run on thread pool if needed
    co_return process_data(data);
}
```

### 11.2 Gradual Adoption

**Phase 1**: Replace event loops
```cpp
// Old: callback-based
event_loop.on_read(fd, callback);

// New: coroutine-based
co_await stream.receive(buffer);
```

**Phase 2**: Compose operations
```cpp
// Chain async operations
auto data = co_await read_data();
auto processed = co_await process(data);
co_await write_result(processed);
```

---

## 12. Platform Considerations

### 12.1 Kernel Version Detection

```cpp
struct KernelVersion {
    int major, minor, patch;
    
    static KernelVersion detect() {
        utsname uts;
        uname(&uts);
        // Parse uts.release
    }
    
    bool supports_multishot_accept() const {
        return (major > 5) || (major == 5 && minor >= 19);
    }
};
```

### 12.2 Feature Probing

```cpp
struct IoUringCapabilities {
    bool multishot_accept;
    bool provided_buffers;
    bool registered_files;
    
    static IoUringCapabilities probe(io_uring* ring) {
        io_uring_probe* probe = io_uring_get_probe_ring(ring);
        // Check supported operations
        io_uring_free_probe(probe);
    }
};
```

---

## 13. Conclusion

This document provides technical guidance for implementing the coroutine-based io_uring library. Key takeaways:

1. **Layered Architecture**: Clear separation of concerns
2. **RAII Everywhere**: Automatic resource management
3. **Type Safety**: Leverage C++20 type system
4. **Zero-Cost**: Abstractions compile to optimal code
5. **Performance**: Batching, zero-copy, caching
6. **Testability**: Unit and integration testing strategies
7. **Debugability**: Debug mode and tracing support

**Next Steps**:
1. Review this architecture
2. Consult API_SPEC.md for detailed interfaces
3. Begin implementation starting with core types
4. Validate with unit tests
5. Build integration tests
6. Performance benchmark

---

## References

1. C++20 Coroutines Deep Dive: https://lewissbaker.github.io/
2. io_uring Performance Guide: https://kernel.dk/io_uring.pdf
3. liburing API Reference: https://github.com/axboe/liburing/tree/master/man
4. Linux io_uring Tutorial: https://unixism.net/loti/
