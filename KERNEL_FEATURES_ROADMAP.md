# io_uring Kernel Features Roadmap

**Last Updated**: January 2026  
**Target Kernel**: 6.17+ (current development version compatible)  
**Analysis Date**: 2026-01-11

This document tracks io_uring kernel features that could enhance this project, their implementation priority, and status.

---

## Executive Summary

This project currently uses the **foundational io_uring API** without taking advantage of many performance-enhancing features available in kernel 6.x. By adopting modern io_uring capabilities, we can:

- Reduce syscall overhead by 30-50%
- Simplify accept-heavy server implementations
- Enable true zero-copy networking paths
- Improve graceful shutdown and timeout handling

---

## Current Feature Usage

### Features Currently Implemented

| Feature | API Used | Notes |
|---------|----------|-------|
| Basic Setup | `io_uring_queue_init()` | flags=0, no optimization flags |
| SQE/CQE Handling | `get_sqe`, `submit`, `wait_cqe`, `peek_cqe`, `cqe_seen` | Standard polling loop |
| File I/O | `prep_read`, `prep_write` | Basic file operations |
| Network I/O | `prep_accept`, `prep_recv`, `prep_send`, `prep_close` | Single-shot operations only |
| User Data | `set_data`, `get_data` | For coroutine handle association |

### Features NOT Currently Used

- Setup optimization flags (SINGLE_ISSUER, DEFER_TASKRUN, COOP_TASKRUN)
- Multishot operations (accept, recv)
- Provided buffers / buffer rings
- Registered buffers/files
- SQE linking
- Operation cancellation
- Timeout variants

---

## Feature Implementation Roadmap

### Phase 1: Low-Hanging Fruit (Minimal Code Change, High Impact)

#### 1.1 Setup Optimization Flags
**Kernel Requirement**: 6.0+  
**Priority**: ⭐⭐⭐⭐⭐ Critical  
**Status**: 🔴 Not Started  
**Estimated Effort**: 1-2 hours

**Description**: Add optimization flags to `io_uring_queue_init()` that match our single-threaded executor design.

**Flags to Add**:
```c
IORING_SETUP_SINGLE_ISSUER   // Kernel: only one thread submits
IORING_SETUP_DEFER_TASKRUN   // Defer work to userspace poll
IORING_SETUP_COOP_TASKRUN    // Cooperative task running
```

**Code Changes**:
- `IoUring::create()` in both `04_coroutine_io` and `04a_coroutine_io_cpp23`
- Add fallback for older kernels

**Benefits**:
- Reduces context switches
- Better CPU cache utilization
- Zero changes to application code

**Files to Modify**:
- `examples/04_coroutine_io/include/io_uring_async/core/io_uring_wrapper.hpp`
- `examples/04a_coroutine_io_cpp23/include/io_uring_async_cpp23/core/io_uring_wrapper.hpp`

---

#### 1.2 Submit-and-Wait with Timeout
**Kernel Requirement**: 6.0+  
**Priority**: ⭐⭐⭐⭐ High  
**Status**: 🔴 Not Started  
**Estimated Effort**: 2-3 hours

**Description**: Replace `io_uring_wait_cqe()` with timeout variant for better control flow.

**API**:
```c
io_uring_submit_and_wait_timeout(ring, &cqe, 1, &timeout, NULL);
```

**Benefits**:
- Enables periodic housekeeping tasks
- Required for graceful shutdown
- Enables timer-based operations

**Files to Modify**:
- `examples/04_coroutine_io/include/io_uring_async/runtime/executor.hpp`

---

### Phase 2: Network Optimizations (Medium Effort, High Impact)

#### 2.1 Multishot Accept
**Kernel Requirement**: 5.19+  
**Priority**: ⭐⭐⭐⭐⭐ Critical for servers  
**Status**: 🔴 Not Started  
**Estimated Effort**: 4-6 hours

**Description**: Submit one accept SQE that generates unlimited CQEs for incoming connections.

**API**:
```c
io_uring_prep_multishot_accept(sqe, fd, addr, addrlen, 0);
// Check: cqe->flags & IORING_CQE_F_MORE
```

**Code Changes**:
- New `MultishotAcceptAwaitable` class
- Modify `TcpListener` to use multishot mode
- Handle `IORING_CQE_F_MORE` flag

**Benefits**:
- One SQE handles unlimited accepts
- Reduces SQ pressure on accept-heavy servers
- Simpler TcpListener implementation

**Files to Modify**:
- `examples/04_coroutine_io/include/io_uring_async/runtime/awaitable.hpp`
- `examples/04_coroutine_io/include/io_uring_async/io/tcp_listener.hpp`
- `examples/04_coroutine_io/src/io/tcp_listener.cpp`

---

#### 2.2 Cancel Operations
**Kernel Requirement**: 5.5+ (improved in 6.12+)  
**Priority**: ⭐⭐⭐⭐ High  
**Status**: 🔴 Not Started  
**Estimated Effort**: 3-4 hours

**Description**: Add ability to cancel pending operations.

**API**:
```c
io_uring_prep_cancel64(sqe, user_data, 0);  // Cancel by user_data
io_uring_prep_cancel_fd(sqe, fd, 0);         // Cancel by fd
```

**Code Changes**:
- Add `prep_cancel()` to IoUring wrapper
- Add `CancelAwaitable` class
- Integrate with TcpStream close()

**Benefits**:
- Proper cleanup on connection close
- Required for timeout implementation
- Graceful shutdown support

**Files to Modify**:
- `examples/04_coroutine_io/include/io_uring_async/core/io_uring_wrapper.hpp`
- `examples/04_coroutine_io/include/io_uring_async/runtime/awaitable.hpp`

---

#### 2.3 SQE Linking
**Kernel Requirement**: 5.3+  
**Priority**: ⭐⭐⭐ Medium  
**Status**: 🔴 Not Started  
**Estimated Effort**: 2-3 hours

**Description**: Chain operations that depend on each other.

**API**:
```c
sqe->flags |= IOSQE_IO_LINK;      // Cancel chain on failure
sqe->flags |= IOSQE_IO_HARDLINK;  // Continue chain on failure
```

**Use Cases**:
- Read then write (echo pattern)
- Operation with timeout
- Atomic multi-step operations

**Files to Modify**:
- `examples/04_coroutine_io/include/io_uring_async/core/io_uring_wrapper.hpp`

---

### Phase 3: High-Performance Features (Higher Effort, Highest Impact)

#### 3.1 Provided Buffers / Buffer Rings
**Kernel Requirement**: 5.7+ (buffer rings in 5.19+)  
**Priority**: ⭐⭐⭐⭐ High  
**Status**: 🔴 Not Started  
**Estimated Effort**: 8-12 hours

**Description**: Kernel manages a buffer pool for receive operations.

**API**:
```c
// Setup buffer ring
struct io_uring_buf_ring *br = io_uring_setup_buf_ring(ring, entries, bgid, 0, &ret);
io_uring_buf_ring_add(br, buf, buf_len, bid, mask, offset);
io_uring_buf_ring_advance(br, 1);

// Use in recv:
sqe->buf_group = bgid;
sqe->flags |= IOSQE_BUFFER_SELECT;
// cqe->flags contains buffer ID
```

**Code Changes**:
- New `BufferRing` class
- Modified recv awaitable for buffer selection
- Buffer recycling mechanism

**Benefits**:
- Zero-copy receive path
- Kernel picks optimal buffer
- Works with multishot recv

**Files to Create**:
- `examples/04_coroutine_io/include/io_uring_async/core/buffer_ring.hpp`

---

#### 3.2 Multishot Recv
**Kernel Requirement**: 6.0+ (improved 6.8+)  
**Priority**: ⭐⭐⭐⭐ High  
**Status**: 🔴 Not Started  
**Estimated Effort**: 6-8 hours  
**Depends On**: 3.1 (Provided Buffers)

**Description**: Single recv SQE handles multiple incoming messages.

**API**:
```c
io_uring_prep_recv_multishot(sqe, fd, NULL, 0, 0);
// Must use provided buffers
sqe->buf_group = bgid;
sqe->flags |= IOSQE_BUFFER_SELECT;
```

**Benefits**:
- One SQE for entire connection lifetime
- Combines with provided buffers for zero-copy
- Ideal for long-lived connections

---

#### 3.3 Registered Buffers
**Kernel Requirement**: 5.1+  
**Priority**: ⭐⭐⭐ Medium  
**Status**: 🔴 Not Started  
**Estimated Effort**: 4-6 hours

**Description**: Pre-register buffers with kernel to avoid per-operation memory pinning.

**API**:
```c
io_uring_register_buffers(ring, iovecs, nr_iovecs);
io_uring_prep_read_fixed(sqe, fd, buf, len, offset, buf_index);
```

**Benefits**:
- Faster I/O for known buffer sets
- Reduced kernel overhead
- True zero-copy possible

---

### Phase 4: Advanced Features (Future)

#### 4.1 NAPI Busy Polling
**Kernel Requirement**: 6.7+  
**Priority**: ⭐⭐ Low  
**Status**: 🔴 Not Started

**Description**: Integrate with network driver busy polling for lowest latency.

---

#### 4.2 CQE_F_SOCK_NONEMPTY Hint
**Kernel Requirement**: 6.5+  
**Priority**: ⭐⭐ Low  
**Status**: 🔴 Not Started

**Description**: Use kernel hint that more data is available in socket buffer.

---

#### 4.3 Futex Operations
**Kernel Requirement**: 6.7+ (improved 6.10+)  
**Priority**: ⭐ Low  
**Status**: 🔴 Not Started

**Description**: Use io_uring for futex operations (userspace synchronization).

---

## Implementation Guidelines

### Feature Detection

Always check for feature support at runtime:
```c
// Check kernel capabilities
struct io_uring_params params;
memset(&params, 0, sizeof(params));
params.flags = IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_DEFER_TASKRUN;

int ret = io_uring_queue_init_params(queue_depth, &ring, &params);
if (ret < 0) {
    // Fall back to basic initialization
    ret = io_uring_queue_init(queue_depth, &ring, 0);
}

// Check for specific features
if (ring.features & IORING_FEAT_NATIVE_WORKERS) { ... }
```

### Fallback Strategy

1. **Best effort**: Try advanced features, fall back silently
2. **Log degradation**: Warn user when running in compatibility mode
3. **Fail fast**: For required features, fail clearly

### Testing Strategy

1. Unit tests with feature mocking
2. Integration tests on target kernel (6.17+)
3. Compatibility tests on older kernels (5.19+, 6.0+, 6.5+)

---

## References

- [io_uring master PDF](https://kernel.dk/io_uring.pdf)
- [liburing repository](https://github.com/axboe/liburing)
- [LWN io_uring coverage](https://lwn.net/Kernel/Index/#io_uring)
- [Kernel 6.x release notes](https://kernelnewbies.org/LinuxVersions)

---

## Changelog

### 2026-01-11
- Initial roadmap created from kernel feature analysis
- Documented current feature usage
- Identified 10+ enhancement opportunities
- Prioritized into 4 implementation phases
