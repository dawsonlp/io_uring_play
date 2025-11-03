# Design Decisions Log

**Project**: C++20 Coroutine-Based io_uring Library  
**Purpose**: Track architectural and implementation decisions  
**Format**: Chronological log of decisions, rationale, and alternatives

---

## 2025-10-30: Initial Design Phase

### Decision 1: Target C++20 Instead of C++23

**Context**: Need to choose language standard version for the library

**Decision**: Use C++20 with minimal std::expected-like implementation

**Rationale**:
- Coroutines (the core feature) are available in C++20
- Broader compiler support (GCC 11+, Clang 14+ vs GCC 13+, Clang 17+)
- More developers can use the library
- C++20 is fully ratified and stable
- Missing C++23 features (std::expected) can be implemented internally

**Alternatives Considered**:
1. C++23: Better features but limited adoption and compiler support
2. C++17: No coroutines, would require callback-based design

**Trade-offs**:
- ✓ Wider compatibility
- ✓ Stable toolchain
- ✗ Must implement Result<T,E> ourselves (not std::expected)
- ✗ Missing some C++23 conveniences (deducing this, etc.)

**Implementation Impact**: 
- Need to create custom Result<T,E> type
- Can upgrade to std::expected when C++23 becomes standard

---

### Decision 2: Single-Threaded Executor Model

**Context**: Need to decide threading model for the event loop

**Decision**: Single-threaded executor with support for running multiple independent executors

**Rationale**:
1. **Simplicity**: No synchronization primitives needed
2. **Performance**: No lock contention, better cache locality
3. **Reasoning**: Easier to understand, debug, and maintain
4. **io_uring fit**: io_uring instances are not thread-safe anyway
5. **Educational**: Clearer for learning purposes

**Alternatives Considered**:
1. Thread pool executor: More complex, requires synchronization
2. Thread-per-core model: Good for production but complex for education
3. Work stealing: Very complex, difficult to implement correctly

**Trade-offs**:
- ✓ Simple programming model
- ✓ No data races possible
- ✓ Predictable performance
- ✗ Single-core utilization per executor
- ✗ Users must run multiple executors for multi-core (explicit parallelism)

**Implementation Impact**:
- No mutexes or atomics needed in executor
- Users can run multiple executors in different threads
- Future: Can add thread-per-core executor as separate class

---

### Decision 3: Value-Semantic Tasks (Move-Only)

**Context**: Need to decide ownership model for Task<T> objects

**Decision**: Tasks are movable values with unique ownership

**Rationale**:
1. **Clear ownership**: No ambiguity about who owns coroutine frame
2. **Composability**: Easy to return from functions and pass around
3. **Performance**: No atomic reference counting operations
4. **Safety**: Compiler enforces correct usage through type system
5. **Zero-cost**: Same as hand-written code

**Alternatives Considered**:
1. Shared ownership (reference counted): More flexible but atomic overhead
2. Intrusive reference counting: Complex, easy to misuse
3. Observer pattern: Lifetime management complexity

**Trade-offs**:
- ✓ Zero overhead vs manual code
- ✓ Clear semantics
- ✓ No circular reference issues
- ✗ Can't share Tasks between multiple waiters
- ✗ Some move overhead when passing around

**Implementation Impact**:
- Task class is move-only (deleted copy constructor)
- Must use std::move when passing Tasks
- Coroutine frame has single owner

---

### Decision 4: Explicit Result<T, E> Error Handling

**Context**: Need to choose error handling strategy

**Decision**: Use Result<T, E> type for explicit error handling, not exceptions

**Rationale**:
1. **Explicit**: Errors are part of function signature
2. **Performance**: No exception overhead in hot path
3. **Predictability**: No hidden control flow
4. **Composability**: Works well with coroutines and co_await
5. **Forced checking**: [[nodiscard]] prevents ignoring errors

**Alternatives Considered**:
1. Exceptions: Better for rare errors but performance overhead
2. Error codes (C-style): Easy to ignore, not type-safe
3. Hybrid (Result + exceptions): More complex, inconsistent

**Trade-offs**:
- ✓ Clear error paths in code
- ✓ No exception overhead
- ✓ Cannot accidentally ignore errors
- ✗ More verbose than exceptions
- ✗ Must explicitly check every Result

**Implementation Impact**:
- All I/O operations return Result<T, Error>
- Must use co_return std::unexpected(error) for errors
- Can provide .value() method that throws for convenience

---

### Decision 5: Lazy Task Evaluation

**Context**: Decide when coroutines should start executing

**Decision**: Tasks are lazy - they don't start until awaited

**Rationale**:
1. **Composability**: Can create tasks without starting them
2. **Control**: Caller decides when execution begins
3. **Resource management**: Don't allocate until needed
4. **Standard pattern**: Matches other coroutine libraries

**Alternatives Considered**:
1. Eager evaluation: Start immediately on creation
2. Explicit start method: More control but more ceremony

**Trade-offs**:
- ✓ Better composability
- ✓ Caller controls execution
- ✗ Slightly less intuitive for beginners
- ✗ Must remember to await created tasks

**Implementation Impact**:
- promise_type::initial_suspend() returns std::suspend_always
- Task doesn't start until co_awaited
- Must document this behavior clearly

---

### Decision 6: Multiple Buffer Management Strategies

**Context**: Need to support different buffer usage patterns

**Decision**: Provide three buffer management strategies based on use case

**Strategies**:
1. **User-Provided Buffers**: User allocates, library borrows via std::span
2. **Registered Buffers**: Pre-register with kernel for zero-copy
3. **Provided Buffers**: Kernel manages buffer pool (future enhancement)

**Rationale**:
- Different use cases have different requirements
- Performance vs simplicity trade-off
- Allow optimization for specific scenarios
- Progressive disclosure (simple default, advanced options)

**Trade-offs**:
- ✓ Flexibility for different use cases
- ✓ Can optimize per scenario
- ✓ Performance when needed
- ✗ More API surface area
- ✗ Users must understand options

**Implementation Impact**:
- TcpStream::receive() takes std::span (user-provided)
- TcpStream::receive_registered() uses buffer IDs
- Future: TcpStream::receive_provided() for kernel-managed buffers

---

### Decision 7: Batched Operation Submission

**Context**: io_uring_submit() is a syscall that should be minimized

**Decision**: Batch submissions automatically in executor

**Rationale**:
- Each io_uring_submit() is expensive (syscall)
- Batching can reduce syscalls by 10-20x
- Automatic batching is transparent to user
- Critical for high-performance workloads

**Strategy**:
- Accumulate operations in submission queue
- Submit when batch size reached (default 128)
- Submit when no more ready work
- Force submit before waiting for completions

**Implementation Impact**:
- Executor tracks pending submission count
- maybe_submit() checks if batch is full
- force_submit() called before io_uring_wait_cqe()
- User doesn't need to think about batching

---

### Decision 8: Educational Focus Over Feature Completeness

**Context**: Balance between comprehensive features and code clarity

**Decision**: Prioritize code clarity and educational value over feature completeness

**Rationale**:
1. Primary goal is to teach coroutine-based I/O
2. Production libraries (Boost.Asio, Seastar) already exist
3. Clear code is more valuable for learning
4. Can add features incrementally
5. Reduces maintenance burden

**What This Means**:
- Implement core features well, not all features
- Extensive comments and documentation
- Simple, readable implementations
- Examples for every major feature
- Not targeting production deployment initially

**Trade-offs**:
- ✓ Easier to understand and learn from
- ✓ Maintainable codebase
- ✓ Clear examples
- ✗ Not feature-complete vs production libraries
- ✗ May need to use other libraries for production

---

### Decision 9: Linux-Only, No Cross-Platform Support

**Context**: Whether to support multiple platforms

**Decision**: Linux-only, optimized for io_uring

**Rationale**:
1. io_uring is Linux-specific by nature
2. Cross-platform abstraction would hide io_uring capabilities
3. No need to support lowest common denominator
4. Can demonstrate modern Linux kernel features
5. Matches educational goal (show best practices on Linux)

**Alternatives Considered**:
- Cross-platform abstraction layer (like Boost.Asio)
- Multiple backends (io_uring, epoll, IOCP)

**Trade-offs**:
- ✓ Can use all io_uring features
- ✓ No abstraction overhead
- ✓ Simpler implementation
- ✗ Linux-only (not a con for our use case)
- ✗ Can't use on Windows/macOS

**Implementation Impact**:
- No platform abstraction layer
- Direct io_uring API usage
- Can use multishot, provided buffers, etc.
- Document Linux kernel version requirements

---

### Decision 10: Namespace Organization

**Context**: How to organize code into namespaces

**Decision**: Use hierarchical namespace structure

**Structure**:
```cpp
namespace io_uring_async {
    namespace core {      // Task, Result, Concepts
    }
    namespace runtime {   // Executor, Awaitable
    }
    namespace io {        // TcpListener, TcpStream
    }
}
```

**Rationale**:
- Clear separation of concerns
- Easy to understand component relationships
- Follows C++ best practices
- Allows selective imports

**Implementation Impact**:
- Users can `using namespace io_uring_async::io;` for I/O types
- Internal organization matches directory structure
- Convenience header includes all namespaces

---

## Future Decisions to Make

The following decisions will be made during implementation:

1. **Coroutine Frame Allocation**: Custom allocator vs compiler optimization?
2. **Timeout Implementation**: Linked timeouts vs separate operations?
3. **Multishot Operations**: When to use multishot vs regular operations?
4. **Error Type Design**: Single Error type vs hierarchy?
5. **Testing Framework**: Which unit test framework to use?
6. **Benchmark Strategy**: What metrics to measure and how?

---

## Decision Review Process

Decisions will be reviewed:
- After initial implementation (validate assumptions)
- When performance benchmarks are available
- Based on user feedback
- Before any major version release

Each decision can be revisited if:
- New information becomes available
- Requirements change
- Better alternatives emerge
- Implementation reveals issues

---

## References

- DESIGN.md: High-level architectural decisions and rationale
- ARCHITECTURE_NOTES.md: Technical implementation guidance
- COMPETITIVE_ANALYSIS.md: How we differ from existing libraries
