# Coroutine I/O with C++23 and io_uring

This directory contains an educational C++23 runtime built directly on
`liburing`. It demonstrates coroutine suspension, stable completion routing,
TCP stream operations, cancellation during executor shutdown, and explicit
resource-lifetime rules.

## Project status

The example is under active architectural development. It is tested and useful
for learning, experiments, and design review, but it is not presented as a
production-ready networking library or a performance reference.

Implemented and tested:

- `Task<T>` and `Task<void>` coroutine types with nested exception propagation.
- A heap-owned, non-movable, single-thread-confined executor.
- Stable `OperationState` ownership and direct CQE routing.
- File read and write awaitables with offsets and EOF behavior.
- IPv4 and IPv6 TCP listeners, accept, send, receive, and `send_all()`.
- Deferred descriptor closure while an operation retains a descriptor reference.
- Executor-wide graceful shutdown using async cancellation and CQE draining.
- A thread-safe `request_stop()` wake path using `eventfd`.
- Debug, release, AddressSanitizer, and UndefinedBehaviorSanitizer test modes.
- Kernel-independent runtime-state tests and live-kernel integration tests.

Not implemented or not yet established:

- Public per-operation, per-task, or per-connection cancellation APIs.
- Mechanically enforced buffer ownership; asynchronous APIs accept non-owning
  spans whose storage lifetime remains the caller's responsibility.
- Cross-thread task submission or coroutine execution.
- TLS, timers, multishot accept, provided-buffer rings, registered resources,
  zero-copy operations, or an asynchronous close API.
- Runtime opcode/feature probing and fallback selection.
- A declared minimum Linux kernel, `liburing`, or GCC version.
- Reproducible performance baselines or performance claims.

## Documentation map

Read only as far as your goal requires:

| Document | Read it when you need to understand |
| --- | --- |
| This README | Status, build commands, limitations, and the public entry point |
| [IO_SEMANTICS.md](IO_SEMANTICS.md) | Partial transfers, EOF, socket flags, errors, and close behavior |
| [LIFETIME_SEMANTICS.md](LIFETIME_SEMANTICS.md) | Who owns executors, tasks, buffers, descriptors, and shutdown work |
| [OPERATION_LIFECYCLE.md](OPERATION_LIFECYCLE.md) | How one SQE moves from preparation through CQE consumption and resumption |
| [DESIGN_CPP23.md](DESIGN_CPP23.md) | How the implemented modules fit together |
| [design_decisions.md](design_decisions.md) | Why the current architectural boundaries were chosen |
| [ARCHITECTURAL_CODE_QUALITY_CHECKLIST.md](ARCHITECTURAL_CODE_QUALITY_CHECKLIST.md) | Completed work, open work, deferrals, and the definition of norm-setting |

For kernel-version context across the repository, see the
[io_uring kernel feature matrix](../../IO_URING_KERNEL_FEATURE_MATRIX.md).

The semantic and lifecycle documents are normative: code and examples should
match them. The design documents explain the implementation. The checklist is
the roadmap; unchecked entries are not capabilities.

Terminology used throughout:

- **operation** means one prepared asynchronous kernel request and its stable
  `OperationState`;
- **task** means a C++ coroutine represented by `Task<T>`;
- **root task** means a task owned directly by the executor through `spawn()`;
- **owner thread** means the thread that created the executor;
- **shutdown** means the executor-wide cancellation and drain sequence;
- **close** means releasing descriptor ownership, which may defer the numeric
  descriptor close until in-flight operations finish.

## Requirements

- Linux with `io_uring` available to the process.
- `liburing` development headers and library.
- A compiler capable of compiling the code as C++23, including
  `std::expected` and `std::print`.
- GNU Make.

The minimum supported versions have not been established. The most recent local
verification used GCC 15.2.0, `liburing` 2.15 built from the
[official release](https://github.com/axboe/liburing/releases/tag/liburing-2.15),
and Linux 7.0.0. The release, debug, ASan, and UBSan matrix and the shutdown and
routing stress suites passed against that build. These versions describe a
verified development environment, not minimum requirements.

## Build and test

From this directory:

```bash
make MODE=debug build
make MODE=debug test
make matrix
```

`make matrix` builds and runs release, debug, ASan, and UBSan configurations in
separate output directories.

The unit suite does not require a working `io_uring` instance. Integration tests
probe ring creation before running. If the host kernel, permissions, or seccomp
policy prevents ring creation, affected integration tests report `SKIP` with exit
status 77; other initialization and behavioral failures still fail the suite.

Additional stress coverage is available with:

```bash
make MODE=asan STRESS_RUNS=25 stress
```

ASan test runs explicitly enable leak detection. The resource integration test
also compares open file-descriptor counts before and after executor, socket,
listener, eventfd, and ring teardown.

## Echo example

```bash
make MODE=debug examples
./build/debug/bin/echo_server          # port 8080
./build/debug/bin/echo_server 9000
```

The example handles one connected client at a time. It demonstrates the public
TCP API but is not a concurrent or production-shaped server. Port parsing is
strict: only decimal values from 1 through 65535 are accepted.

## API outline

```cpp
auto executor = IoUringExecutor::create();
if (!executor) {
    return 1;
}

auto& runtime = **executor;
runtime.spawn(application(runtime));

if (auto result = runtime.run(); !result) {
    // Observe executor, root-task, submission, or completion failure.
}
```

The executor and all bound listeners, streams, tasks, and awaiters must remain on
the executor's creating thread. Only `request_stop()` may be called from another
thread.

`ReadAwaitable`, `WriteAwaitable`, `SendAwaitable`, and `RecvAwaitable` borrow
their buffers. Backing storage must remain alive and at a stable address until
the await completes or executor teardown has released the kernel request.

## Source layout

```text
include/io_uring_async_cpp23/
  core/       errors, tasks, descriptors, and the ring wrapper
  runtime/    operation state, awaitables, and executor declarations
  io/         TCP listener and stream abstractions
src/runtime/  non-template runtime implementation
examples/     echo server and example-local option parsing
tests/unit/   kernel-independent contracts and deterministic state tests
tests/integration/ live io_uring, file, TCP, cancellation, and lifetime tests
tests/support/ always-on checks and kernel availability gate
```

## Development roadmap

[ARCHITECTURAL_CODE_QUALITY_CHECKLIST.md](ARCHITECTURAL_CODE_QUALITY_CHECKLIST.md)
is the authoritative roadmap. Unchecked items are not current features. In
particular, portability policy and performance work remain incomplete, and the
C++23 implementation has not yet been formally designated as the repository's
canonical implementation.
