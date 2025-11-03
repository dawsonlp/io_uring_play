# Design Document: C++20 Coroutine-Based io_uring Library

**Project**: Async I/O Library with Coroutines and io_uring  
**Author**: Systems Architect  
**Date**: 2025-10-30  
**Version**: 1.0  
**Status**: Design Phase

---

## 1. Executive Summary

### 1.1 Purpose

This project demonstrates a modern approach to asynchronous I/O on Linux by combining C++20 coroutines with the io_uring kernel interface. The library serves both educational and practical purposes, showing how to build a production-quality async runtime from first principles.

### 1.2 Strategic Goals

1. **Educational Excellence**: Serve as a reference implementation for coroutine-based I/O
2. **Performance Leadership**: Match or exceed traditional async I/O approaches
3. **Developer Experience**: Provide clean, intuitive APIs that look synchronous
4. **Type Safety**: Leverage C++20's type system to prevent entire classes of errors
5. **Linux Optimization**: Take full advantage of modern kernel capabilities

### 1.3 Success Criteria

| Criterion | Target | Measurement |
|-----------|--------|-------------|
| Code Clarity | 80%+ developer comprehension | Survey of 10+ engineers |
| Performance | Within 10% of raw liburing | Benchmark comparison |
| Memory Safety | Zero leaks in test suite | Valgrind analysis |
| API Usability | <100 lines for echo server | Example implementation |
| Compile Time | <5s for example app | Measured build time |

---

## 2. Problem Space

### 2.1 The Async I/O Challenge

Traditional approaches to handling thousands of concurrent connections face fundamental trade-offs:

**Blocking I/O + Threads**
- ✓ Simple programming model
- ✗ Poor scalability (memory, context switches)
- ✗ Difficult synchronization

**Event Loops (epoll/select)**
- ✓ Excellent scalability
- ✗ Callback complexity ("callback hell")
- ✗ Scattered control flow
- ✗ Difficult error handling

**Traditional Async Frameworks**
- ✓ Better than raw callbacks
- ✗ Often callback-based internally
- ✗ Heavy abstraction layers
- ✗ Cross-platform compromises

### 2.2 The Modern Solution Space

**io_uring** (Linux 5.1+, mature in 6.17)
- Unified async interface for all I/O
- Shared ring buffers minimize syscalls
- True zero-copy capabilities
- Batch submission and completion
- Advanced features (multishot, provided buffers)

**C++20 Coroutines**
- Native language support for async/await
- Zero runtime overhead
- Sequential-looking async code
- Natural error handling
- Composable operations

### 2.3 Our Unique Position

As documented in `COMPETITIVE_ANALYSIS.md`, we differentiate by:

1. **Coroutine-Native Design**: Built around co_await from day one, not retrofitted
2. **io_uring First**: Exposing kernel capabilities without abstraction compromises
3. **Educational Mission**: Code clarity prioritized over feature completeness
4. **Modern C++ Only**: No legacy compatibility burden
5. **Linux-Optimized**: No cross-platform compromises

---

## 3. Architectural Vision

### 3.1 Core Philosophy

**Principle 1: Zero-Cost Abstractions**
- Every abstraction compiles down to what you'd write by hand
- No virtual dispatch in hot paths
- No heap allocations in steady state

**Principle 2: Type Safety**
- Use types to prevent errors at compile time
- Make incorrect usage impossible to express
- Leverage concepts for clear error messages

**Principle 3: RAII Everything**
- No manual resource management
- Automatic cleanup on all paths
- Exception-safe by construction

**Principle 4: Progressive Disclosure**
- Simple things are simple
- Complex things are possible
- Power users can access raw io_uring

**Principle 5: Local Reasoning**
- Code reads sequentially despite being async
- No hidden control flow
- Clear ownership semantics

### 3.2 Architectural Patterns

**Single-Threaded Async Runtime**
- Rationale: Eliminates synchronization complexity
- Trade-off: Cannot utilize multiple cores without multiple executors
- Alternative considered: Thread-per-core model (future enhancement)

**Value-Semantic Tasks**
- Rationale: Composable, movable, clear ownership
- Trade-off: Some copying overhead vs reference counting
- Alternative considered: Shared ownership with intrusive pointers

**Lazy Evaluation**
- Rationale: Better composability, enables batching, caller controls execution
- Trade-off: Slightly less intuitive for beginners, must remember to await
- Alternative considered: Eager evaluation (simpler but breaks batching)

**Result<T, E> Error Handling**
- Rationale: Explicit error handling, no exceptions in hot path
- Trade-off: More verbose than exceptions
- Alternative considered: Exceptions (available for non-critical paths)

---

## 4. System Architecture

### 4.1 Conceptual Layers

```
┌─────────────────────────────────────────┐
│     Application Layer                    │  User's async code
│  (Echo server, HTTP server, etc.)       │
└──────────────┬──────────────────────────┘
               │ Uses Task<T> API
┌──────────────▼──────────────────────────┐
│     High-Level I/O Components           │  TcpListener, TcpStream
│  (Networking, File I/O)                 │  File, Timer, etc.
└──────────────┬──────────────────────────┘
               │ Returns Task<T>
┌──────────────▼──────────────────────────┐
│     Coroutine Runtime                   │  Task, Promise, Awaitable
│  (Coroutine lifecycle management)       │  Suspension/resumption
└──────────────┬──────────────────────────┘
               │ Schedules on
┌──────────────▼──────────────────────────┐
│     Executor (Event Loop)               │  IoUringExecutor
│  (Operation scheduling & completion)    │  Batching, polling
└──────────────┬──────────────────────────┘
               │ Uses
┌──────────────▼──────────────────────────┐
│     io_uring Wrapper                    │  Type-safe operations
│  (RAII, Result<T>, safety)              │  Error handling
└──────────────┬──────────────────────────┘
               │ Calls
┌──────────────▼──────────────────────────┐
│     liburing / Kernel                   │  System interface
└─────────────────────────────────────────┘
```

### 4.2 Component Responsibilities

**Application Layer**
- Implements business logic
- Composes async operations with co_await
- Handles domain-specific errors

**I/O Components**
- Provide high-level async I/O abstractions
- Manage file descriptors and buffers
- Translate between domain concepts and io_uring operations

**Coroutine Runtime**
- Manages coroutine lifecycle (create, suspend, resume, destroy)
- Provides Task<T> type for composability
- Handles coroutine state and continuations

**Executor**
- Runs the event loop
- Schedules coroutines
- Submits operations to io_uring
- Processes completions and resumes coroutines

**io_uring Wrapper**
- RAII management of io_uring resources
- Type-safe operation preparation
- Error handling and result extraction

### 4.3 Data Flow

**Request Initiation**
```
User code calls async operation (e.g., socket.receive())
  → Returns Task<Result<size_t>>
  → User co_awaits the Task
  → Coroutine suspends
  → Task's await_suspend() called
  → Operation submitted to Executor
  → Executor queues to io_uring SQ
```

**Request Completion**
```
Kernel completes operation
  → CQE appears in io_uring CQ
  → Executor polls CQE
  → Extracts result and operation ID
  → Finds suspended coroutine handle
  → Resumes coroutine
  → Task's await_resume() returns result
  → User code continues with result
```

---

## 5. Key Design Decisions

### 5.1 Language Version: C++20

**Decision**: Target C++20, not C++23

**Rationale**:
1. **Core feature available**: Coroutines are in C++20
2. **Wider adoption**: More developers can use it
3. **Stable standard**: Fully ratified and implemented
4. **Compiler support**: GCC 10+, Clang 14+ vs newer versions
5. **Missing features manageable**: Can implement Result<T, E> ourselves

**Trade-offs**:
- ✓ Broader compatibility
- ✓ Stable toolchain
- ✗ Missing std::expected (implement alternative)
- ✗ No deducing this (minor convenience)

**Alternatives Considered**:
- C++23: Better features but limited adoption
- C++17: No coroutines, callback-based only

### 5.2 Executor Model: Single-Threaded

**Decision**: Single-threaded executor with explicit multi-executor support

**Rationale**:
1. **Simplicity**: No synchronization primitives needed
2. **Performance**: No lock contention, cache-friendly
3. **Reasoning**: Easier to understand and debug
4. **io_uring fit**: io_uring instances are not thread-safe anyway

**Trade-offs**:
- ✓ Simple programming model
- ✓ No data races possible
- ✓ Predictable performance
- ✗ Single-core utilization per executor
- ✗ Must run multiple executors for multi-core

**Alternatives Considered**:
- Thread pool: More complex, synchronization overhead
- Thread-per-core: Future enhancement, keep option open
- Work stealing: Complex, difficult to implement correctly

### 5.3 Memory Management: Value Semantics

**Decision**: Tasks are movable values, not reference-counted

**Rationale**:
1. **Clear ownership**: No ambiguity about who owns what
2. **Composability**: Easy to return from functions
3. **Performance**: No atomic refcount operations
4. **Safety**: Compiler enforces correct usage

**Trade-offs**:
- ✓ Zero overhead vs manual code
- ✓ Clear semantics
- ✓ No cycles possible
- ✗ Can't share Tasks (must move)
- ✗ Some copy overhead when moving

**Alternatives Considered**:
- Shared ownership: More flexible but atomic overhead
- Intrusive refcounting: Complex, easy to misuse
- Observers/handles: Lifetime management complexity

### 5.4 Error Handling: Result<T, E>

**Decision**: Explicit Result<T, E> type, not exceptions

**Rationale**:
1. **Explicit**: Errors are part of the type signature
2. **Performance**: No exception overhead in hot paths
3. **Predictability**: No hidden control flow
4. **Composability**: Works well with coroutines

**Trade-offs**:
- ✓ Clear error paths
- ✓ No performance overhead
- ✓ Cannot ignore errors (compilation error)
- ✗ More verbose than exceptions
- ✗ Must check every result

**Alternatives Considered**:
- Exceptions: Better for rare errors but overhead
- Error codes: C-style, easy to ignore
- Hybrid: Result for expected, exceptions for unexpected

### 5.5 Buffer Management Strategy

**Decision**: Multiple strategies based on use case

**Rationale**: Different scenarios have different requirements

**Strategies**:

1. **User-Provided Buffers**: For known-size operations
   - User allocates (stack or heap)
   - Zero abstraction overhead
   - Used for: File I/O, small messages

2. **Registered Buffers**: For zero-copy high performance
   - Pre-registered with kernel
   - True zero-copy possible
   - Used for: High-throughput networking

3. **Provided Buffers**: For dynamic scenarios
   - Kernel manages buffer pool
   - Returns buffer ID with data
   - Used for: Accept loops, unpredictable sizes

**Trade-offs**:
- ✓ Flexibility for different use cases
- ✓ Can optimize per scenario
- ✗ More complexity in API
- ✗ User must choose strategy

---

## 6. Quality Attributes

### 6.1 Performance Targets

| Metric | Target | Rationale |
|--------|--------|-----------|
| Throughput | 1M+ req/sec | Match raw io_uring performance |
| Latency (p50) | <100μs | Competitive with C implementations |
| Latency (p99) | <500μs | Tail latency matters for UX |
| Memory/conn | <1KB | Enable high connection counts |
| CPU overhead | <5% vs raw | Abstractions should be zero-cost |

### 6.2 Reliability Requirements

**Memory Safety**
- No memory leaks (verified by Valgrind)
- No use-after-free (RAII prevents)
- No buffer overruns (span prevents)
- No data races (single-threaded)

**Error Handling**
- All errors surfaced to user
- No silent failures
- Graceful degradation
- Clear error messages

**Resource Management**
- Automatic cleanup on all paths
- Exception-safe by construction
- No resource exhaustion from leaks
- Bounded memory usage

### 6.3 Maintainability Goals

**Code Clarity**
- Self-documenting types
- Minimal magic / hidden behavior
- Clear control flow
- Comprehensive comments

**Testability**
- Unit testable components
- Integration test suite
- Benchmark suite
- Example programs

**Evolvability**
- Loosely coupled components
- Clear extension points
- Stable API surface
- Version compatibility plan

---

## 7. Risk Analysis

### 7.1 Technical Risks

**Risk 1: Compiler Coroutine Support Variations**
- **Impact**: High - Code may not compile
- **Probability**: Medium - C++20 is new
- **Mitigation**: Test on GCC 11+, Clang 14+, document requirements
- **Contingency**: Provide compatibility layer if needed

**Risk 2: io_uring Kernel Version Dependencies**
- **Impact**: Medium - Some features unavailable
- **Probability**: High - Users may have older kernels
- **Mitigation**: Runtime feature detection, graceful fallback
- **Contingency**: Provide epoll backend as fallback

**Risk 3: Coroutine Performance Overhead**
- **Impact**: High - Would violate zero-cost principle
- **Probability**: Low - C++20 designed for zero overhead
- **Mitigation**: Extensive benchmarking, profiling
- **Contingency**: Provide non-coroutine API alongside

**Risk 4: API Complexity**
- **Impact**: Medium - Difficult to use correctly
- **Probability**: Medium - Async APIs are inherently complex
- **Mitigation**: Extensive documentation, examples
- **Contingency**: Provide high-level helpers

### 7.2 Operational Risks

**Risk 1: Debugging Complexity**
- **Impact**: High - Suspended coroutines hard to debug
- **Probability**: High - Known coroutine limitation
- **Mitigation**: Extensive logging, debug mode, documentation
- **Contingency**: Provide debugging utilities

**Risk 2: Error Propagation**
- **Impact**: Medium - Errors might be ignored
- **Probability**: Medium - [[nodiscard]] helps but not perfect
- **Mitigation**: Strict [[nodiscard]], documentation
- **Contingency**: Optional exception throwing mode

---

## 8. Alternatives Considered

### 8.1 Callback-Based API

**Description**: Traditional callback style like Node.js

**Pros**:
- Well understood pattern
- Slightly more efficient (no coroutine overhead)
- Compatible with C++17

**Cons**:
- Callback hell
- Difficult error handling
- Scattered control flow

**Decision**: Rejected - Coroutines provide superior ergonomics

### 8.2 Future/Promise Based

**Description**: Like Seastar or JavaScript Promises

**Pros**:
- Chainable operations
- More familiar to some developers
- Pre-dates coroutines

**Cons**:
- More complex than coroutines
- Requires custom types
- Less composable

**Decision**: Rejected - Coroutines supersede this pattern

### 8.3 Blocking Multi-Threaded

**Description**: Traditional thread-per-connection

**Pros**:
- Simple programming model
- No async complexity
- Works with existing code

**Cons**:
- Poor scalability
- High memory usage
- Context switch overhead

**Decision**: Rejected - Doesn't meet performance goals

### 8.4 epoll-Based Backend

**Description**: Use epoll instead of io_uring

**Pros**:
- Available on older kernels
- Well understood
- Proven at scale

**Cons**:
- More syscalls
- No zero-copy
- Older design

**Decision**: Rejected for primary implementation, but viable fallback

---

## 9. Dependencies & Constraints

### 9.1 Hard Requirements

**Kernel**
- Linux 6.17+ for full feature set
- Linux 5.19+ minimum (acceptable degradation)

**Compiler**
- GCC 11+ or Clang 14+ for C++20 coroutines
- Full C++20 standard library

**Libraries**
- liburing (latest version for kernel)
- Standard C++ library only

### 9.2 Soft Requirements

**Development Tools**
- CMake or Make for builds
- Valgrind for memory checking
- gdb with coroutine support for debugging
- Benchmarking tools (custom or google-benchmark)

**Documentation Tools**
- Markdown for documentation
- Doxygen for API docs (optional)

### 9.3 Constraints

**Platform**: Linux-only by design (not a limitation)
**Language**: C++20 only (no backward compatibility)
**Threading**: Explicitly single-threaded per executor
**Resource Management**: Must use RAII (no manual cleanup)

---

## 10. Future Directions

### 10.1 Near-Term Enhancements

1. **HTTP/1.1 Layer**: Build HTTP server on top of TCP abstractions
2. **TLS Support**: Integration with OpenSSL/BoringSSL
3. **Timer Support**: Async delays and timeouts
4. **File I/O**: Async file operations beyond basic read

### 10.2 Medium-Term Evolution

1. **Thread-Per-Core**: Multiple executor model for multi-core
2. **Zero-Copy Optimizations**: Registered buffers, splice operations
3. **Advanced Features**: Multishot operations, provided buffers
4. **Observability**: Metrics, tracing, debugging tools

### 10.3 Long-Term Vision

1. **HTTP/2 and HTTP/3**: Modern protocols
2. **Influence Standards**: Contribute to C++ networking TS
3. **Production Hardening**: Security audit, fuzzing, formal verification
4. **Framework Ecosystem**: Database clients, message queues, etc.

---

## 11. Conclusion

This design presents a modern, educational, and practical approach to async I/O on Linux by leveraging the synergy between C++20 coroutines and io_uring.

**Key Strengths**:
- Clean, maintainable code through coroutines
- High performance through io_uring
- Type safety through modern C++
- Educational value through clarity

**Key Trade-offs**:
- C++20 requirement (vs broader compatibility)
- Linux-only (vs cross-platform)
- Single-threaded (vs automatic parallelism)
- Result<T,E> (vs exceptions)

**Next Steps**:
1. Review and approve this design
2. Create detailed architecture notes
3. Define API specifications
4. Begin implementation
5. Validate with benchmarks

---

## Appendix A: Terminology

- **Coroutine**: Function that can suspend and resume execution
- **Task**: Type representing a suspendable async operation
- **Awaitable**: Object that can be used with co_await
- **Executor**: Event loop that schedules and runs coroutines
- **SQE**: Submission Queue Entry (io_uring operation request)
- **CQE**: Completion Queue Entry (io_uring operation result)
- **RAII**: Resource Acquisition Is Initialization

## Appendix B: References

1. C++20 Coroutines: https://en.cppreference.com/w/cpp/language/coroutines
2. io_uring: https://kernel.dk/io_uring.pdf
3. Linux 6.17 io_uring features: https://lwn.net/Kernel/Index/#io_uring
4. liburing: https://github.com/axboe/liburing
5. Competitive Analysis: See `COMPETITIVE_ANALYSIS.md` in this repository
