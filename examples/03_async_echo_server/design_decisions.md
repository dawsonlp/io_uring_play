# Design Decisions: C++20 Coroutines + io_uring Async Networking

## Date: 2025-10-20

## Overview
This document logs the architectural and implementation decisions for the async networking example using C++20 coroutines and io_uring.

---

## Decision 1: C++20 Coroutines as Primary API

**Decision**: Use C++20 coroutines (`co_await`, `Task<T>`) as the user-facing API

**Rationale**:
- **Best of both worlds**: Synchronous-looking code with async execution
- **Natural composition**: Async functions compose like sync functions
- **Error handling**: Can use exceptions or `Result<T>`  naturally
- **Industry proven**: Rust's async/await, Python's asyncio show this works
- **Zero overhead**: Coroutines are lightweight (~100 bytes vs. ~8MB threads)

**Trade-offs**:
- Requires C++20 compiler (GCC 10+, Clang 10+)
- Learning curve for developers unfamiliar with coroutines
- Debugging can be harder than traditional code

**Alternatives Considered**:
- **Callbacks**: More efficient but leads to callback hell
- **Futures/Promises**: Verbose, hard to compose
- **Blocking API**: Simple but doesn't scale

---

## Decision 2: Single-Threaded Event Loop

**Decision**: Use single-threaded executor (like Node.js, Redis)

**Rationale**:
- **No locking needed**: Simpler code, fewer bugs
- **Predictable**: Deterministic task execution order
- **Cache-friendly**: Better CPU cache utilization
- **Easier debugging**: No race conditions
- **Proven model**: Node.js, Redis, Nginx worker model

**Trade-offs**:
- Can't utilize multiple cores within single executor
- CPU-intensive tasks block other tasks
- Need process-level parallelism for multi-core

**When to reconsider**:
- If profiling shows CPU as bottleneck (unlikely for I/O-bound)
- If need to handle CPU-intensive tasks (consider separate thread pool)

---

## Decision 3: io_uring for All I/O

**Decision**: Use io_uring for all I/O operations, not epoll/select

**Rationale**:
- **Performance**: 20-30% better than epoll in benchmarks
- **Unified API**: Files, sockets, timers all use same interface
- **Batch operations**: Natural batching for efficiency
- **Zero-copy**: Registered buffers eliminate copies
- **Future-proof**: Linux's future for async I/O

**Kernel requirements**:
- Minimum: 5.1 (basic io_uring)
- Recommended: 5.19+ (direct file descriptors, multishot)
- Target: 6.17 (mature, production-ready)

**Fallback strategy**:
- Check kernel version at runtime
- Could add epoll backend for older kernels (not in MVP)

---

## Decision 4: RAII for Resource Management

**Decision**: All resources managed via RAII (sockets, buffers, ring)

**Rationale**:
- **Safety**: Automatic cleanup, no leaks possible
- **Exception-safe**: Works correctly even with exceptions
- **Clear ownership**: Move semantics make ownership explicit
- **C++ idiomatic**: Follows C++ best practices

**Implementation**:
- `TcpListener` owns listening socket
- `TcpStream` owns connected socket
- `IoUringExecutor` owns io_uring instance
- All move-only (no copying)

---

## Decision 5: Task<T> Return Type

**Decision**: Async functions return `Task<T>`, not raw `std::coroutine_handle`

**Rationale**:
- **Type-safe**: `Task<int>` vs. `Task<string>` caught at compile time
- **RAII**: Task manages coroutine lifetime
- **Composable**: Can await `Task<T>`, return `Task<U>`
- **Familiar**: Similar to Rust's `Future<T>`, JS's `Promise<T>`

**Example**:
```cpp
Task<int> read_value() {
    auto data = co_await stream.receive(buffer);
    co_return parse_int(data);
}

Task<void> process() {
    int value = co_await read_value();  // Type-safe!
    // ...
}
```

---

## Decision 6: No Exceptions in Hot Path

**Decision**: Use `Result<T, Error>` for I/O operations, not exceptions

**Rationale**:
- **Performance**: Zero overhead (just a variant)
- **Explicit**: Forces error checking
- **Composable**: Can chain with `.and_then()`, `.or_else()`

**When exceptions are OK**:
- Initialization/setup (not hot path)
- Resource exhaustion (rare, can't continue)
- Programming errors (should crash)

**Example**:
```cpp
// I/O operation returns Result
Result<size_t> send(span<byte> data);

// Usage
auto result = co_await stream.send(data);
if (!result) {
    // Handle error
    log_error(result.error());
}
```

---

## Decision 7: Eager Submission, Lazy Completion

**Decision**: Submit operations eagerly, but batch completions

**Rationale**:
- **Low latency**: Don't wait to submit
- **High throughput**: Batch completions for efficiency

**Implementation**:
```cpp
// Eagerly submit when co_await is called
co_await stream.send(data);  // Submits immediately

// But executor batches completion processing
executor.run() {
    while (has_work) {
        submit_pending();       // Submit any queued ops
        wait_for_completions(); // Wait for batch
        process_all_ready();    // Process all at once
    }
}
```

---

## Decision 8: Stack-Allocated Buffers by Default

**Decision**: Use stack buffers (`std::array`) by default, not heap

**Rationale**:
- **Performance**: No allocation overhead
- **Locality**: Better cache behavior
- **Simplicity**: No lifetime management

**When to use heap**:
- Large buffers (>16KB)
- Long-lived buffers
- Zero-copy scenarios (registered buffers)

**Example**:
```cpp
Task<void> handle_client(TcpStream client) {
    std::array<byte, 4096> buffer;  // Stack allocated
    while (true) {
        auto data = co_await client.receive(buffer);
        co_await client.send(data);
    }
}
```

---

## Decision 9: No Cancellation in MVP

**Decision**: No task cancellation in initial version

**Rationale**:
- **Complexity**: Cancellation is hard to get right
- **MVP**: Not essential for basic functionality
- **Future**: Can add structured concurrency later

**Workaround**:
- Use timeouts
- Check flags in loops
- Close connections to abort

**Future enhancement**:
```cpp
// Potential future API
auto task = async_operation();
task.cancel();  // Request cancellation
co_await task;  // May throw OperationCancelled
```

---

## Decision 10: Multishot Operations Where Possible

**Decision**: Use multishot accept and receive (kernel 5.19+)

**Rationale**:
- **Efficiency**: One setup, many completions
- **Less syscalls**: Don't re-arm for each operation
- **Kernel feature**: Leverage modern io_uring

**Impact**:
- Accept: One `IORING_OP_ACCEPT` → multiple connections
- Receive: One `IORING_OP_RECV` → multiple receives

**Graceful degradation**:
- Check kernel version
- Fall back to single-shot if not supported

---

## Decision 11: Metrics and Observability Built-In

**Decision**: Include metrics collection from day one

**Rationale**:
- **Production-ready**: Can't deploy without metrics
- **Debugging**: Essential for performance tuning
- **Zero-cost**: Atomic counters are cheap

**Metrics to track**:
- Requests per second
- Bytes sent/received
- Active connections
- Error rate
- Latency histogram (p50, p99, p999)

---

## Decision 12: Minimal Dependencies

**Decision**: Only depend on liburing and C++20 standard library

**Rationale**:
- **Simplicity**: Easy to build and deploy
- **Portability**: Works everywhere C++20 works
- **Control**: We control the entire stack

**Dependencies**:
- liburing: Required for io_uring operations
- C++20 std library: coroutines, span, etc.
- No external libraries (no Boost, etc.)

---

## Implementation Strategy

### Phase 1: Core Runtime (MVP)
1. `Task<T>` promise type
2. Basic executor with event loop
3. Simple I/O operations

**Goal**: Working coroutines with io_uring

### Phase 2: Socket API
1. `TcpListener` with accept
2. `TcpStream` with send/receive
3. Error handling

**Goal**: Working echo server

### Phase 3: Optimization
1. Batching
2. Registered buffers
3. Multishot operations

**Goal**: Production performance

---

## Performance Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| Throughput | 100K req/s | wrk benchmark |
| Latency p50 | <100μs | wrk benchmark |
| Latency p99 | <500μs | wrk benchmark |
| Memory/conn | <1KB | /proc/[pid]/status |
| CPU/conn | <5% | perf stat |

---

## Comparison with Alternatives

### vs. Traditional Sockets + Threads
```
Traditional:
+ Simple API
- High memory (8MB/thread)
- Doesn't scale (>10K conn)
- Context switch overhead

Ours:
+ Low memory (1KB/coro)
+ Scales (>100K conn)
+ Async by design
- Need C++20
```

### vs. epoll + Callbacks
```
epoll:
+ Works on older kernels
- Callback hell
- State machine complexity
- Error handling hard

Ours:
+ Sequential code
+ Natural composition
+ Clean error handling
- Kernel 5.19+ for best perf
```

### vs. Boost.Asio
```
Boost.Asio:
+ Mature ecosystem
+ Cross-platform
- Large dependency
- Complex API
- Callback-based by default

Ours:
+ Native coroutines
+ Direct io_uring
+ Minimal deps
- Linux-only
```

---

## Testing Strategy

1. **Unit tests**: Core runtime components
2. **Integration tests**: Full echo server
3. **Performance tests**: wrk benchmarks
4. **Stress tests**: Long-running, many connections
5. **Valgrind**: Memory leak checking

---

## Security Considerations

1. **Buffer overflows**: Use `span<>` for bounds checking
2. **Resource limits**: Enforce connection limits
3. **DoS protection**: Timeouts, rate limiting
4. **Input validation**: Parse carefully, reject malformed

---

## Future Enhancements

1. **Multi-threading**: Thread pool for CPU-intensive work
2. **Cancellation**: Structured concurrency
3. **Timeouts**: Per-operation and connection timeouts
4. **TLS**: Secure connections
5. **HTTP/2**: Protocol implementation
6. **Metrics export**: Prometheus format
7. **Configuration**: Runtime configuration
8. **Logging**: Structured logging

---

## Lessons Learned
(To be filled during implementation)

### Build Phase
- TBD

### Testing Phase
- TBD

### Performance Tuning
- TBD

---

## Conclusion

These decisions prioritize:
1. **Simplicity**: Single-threaded, minimal deps
2. **Performance**: io_uring, zero-copy, batching
3. **Safety**: RAII, type-safety, no leaks
4. **Maintainability**: Clean code, good errors

The result is a modern async I/O foundation that's both fast and safe.
