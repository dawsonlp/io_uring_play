# Design Decision Record

> [Overview](README.md) · [I/O semantics](IO_SEMANTICS.md) ·
> [Lifetime semantics](LIFETIME_SEMANTICS.md) ·
> [Operation lifecycle](OPERATION_LIFECYCLE.md) · [Design](DESIGN_CPP23.md) ·
> **Decisions** · [Roadmap](ARCHITECTURAL_CODE_QUALITY_CHECKLIST.md)

This file records decisions implemented by the C++23 example. Proposed work
belongs in the architectural checklist and is not described here as current
behavior.

## Use C++23 `std::expected`

Fallible public operations return `std::expected<T, Error>`. This removes the
need for a project-specific result container and gives callers one consistent
value-or-error interface.

This is an API choice, not a performance claim. No benchmark currently compares
the C++20 and C++23 examples.

## Keep operation state at a stable address

Every prepared asynchronous operation receives an executor-owned
`OperationState`. Its address is placed directly in SQE user data and remains
stable through CQE consumption and coroutine resumption.

The executor is heap-owned and non-movable because streams, listeners, awaiters,
and operation states refer to it by address. This trades relocation flexibility
for a simple lifetime rule.

## Confine execution to one owner thread

Submission, task scheduling, completion processing, and executor-bound APIs run
on the executor's creating thread. Debug builds assert this constraint.

`request_stop()` is intentionally different: it may be called from another
thread and wakes a blocked executor through an `eventfd`. Cross-thread task
submission and coroutine execution are not supported.

## Preserve operation-specific io_uring behavior

The ring wrapper centralizes SQE acquisition, queue-full errors, and user-data
attachment, while each preparation method still visibly calls its corresponding
`io_uring_prep_*` function. The runtime shares byte-count result conversion but
keeps accept and composed-transfer policy explicit.

This boundary removes mechanical duplication without turning opcode behavior
into a generic metaprogramming layer.

## Split declarations from runtime mechanics

Template-dependent task and awaitable declarations remain in headers. Executor
event-loop, cancellation, and completion mechanics live in
`src/runtime/executor.cpp`; concrete awaitable behavior lives in
`src/runtime/awaitable.cpp`.

The implementation is therefore not header-only. This split keeps public headers
focused and prevents internal ring mechanics from becoming accidental extension
points.

## Use shared descriptor ownership for in-flight operations

Streams, listeners, and their awaiters share `DescriptorState`. Releasing a
stream or listener does not close a descriptor still named by an operation. The
last owner closes it after the operation reaches its terminal boundary.

An asynchronous close awaitable was rejected because it would make ownership less
clear. Public `close()` is synchronous ownership release; actual descriptor close
may be deferred by an in-flight awaiter.

## Make shutdown the implemented cancellation boundary

Executor shutdown rejects new work, requests kernel cancellation for pending
operations, drains original and cancellation CQEs, and allows task chains to
unwind. Completion and cancellation races resolve the original operation once.

The kernel request is best-effort, while the graceful runtime path continues
until tracked requests are terminal. Ring release is the destructor fallback if
graceful draining fails.

No public API currently cancels one operation, task, or connection independently.
Those scopes must not be inferred from the internal cancellation machinery.

## Keep buffer borrowing explicit

Read, write, send, and receive accept `std::span`. The coroutine caller must keep
the backing storage alive and unmoved until resumption or completed executor
teardown. The runtime does not copy or pin buffers.

Owning or registered-buffer APIs are deferred because they would establish a new
public ownership model rather than merely optimize the existing one.

## Separate deterministic and live-kernel testing

Unit tests do not initialize `io_uring`. A testing-only ring stub allows scripted
submission failure and completion routing to exercise executor state transitions.

Integration tests perform a ring capability gate and use exit status 77 when the
host blocks or lacks `io_uring`. Other initialization errors remain failures.
Live tests cover regular files, TCP, partial transfers, cancellation, shutdown,
descriptor reuse, randomized completion order, and resource teardown.

## Do not claim unmeasured performance

There is no reproducible benchmark suite yet. The project makes no throughput,
latency, allocation, compile-time, code-size, or C++20-parity performance claims.
Optimization work is deferred until the correctness, testing, and portability
phases establish valid measurement conditions.

## Keep canonical status undecided

The C++23 implementation has not yet been formally designated as the repository's
canonical version, and the older C++20 example has not yet been labeled historical
or brought to the same guarantees. That repository-level decision remains an
unchecked documentation-phase item.
