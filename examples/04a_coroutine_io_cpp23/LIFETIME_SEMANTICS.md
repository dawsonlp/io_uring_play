# Lifetime and Cancellation Semantics

> [Overview](README.md) · [I/O semantics](IO_SEMANTICS.md) ·
> **Lifetime semantics** · [Operation lifecycle](OPERATION_LIFECYCLE.md) ·
> [Design](DESIGN_CPP23.md) ·
> [Roadmap](ARCHITECTURAL_CODE_QUALITY_CHECKLIST.md)

This document defines the ownership rules for executor, task, operation, buffer,
and descriptor lifetimes. It distinguishes graceful cancellation from the abrupt
ring-release fallback.

## Executor Lifetime

`IoUringExecutor::create()` returns a heap-owned, non-movable executor. Every
listener, stream, awaiter, and operation associated with an executor must be
destroyed before or as part of that executor's shutdown.

The destructor first stops new work and attempts the same cancellation-and-drain
sequence as `shutdown()`. It then closes the io_uring instance before destroying
any remaining operation states or coroutine frames. Closing the ring invokes the
kernel ring-release path, which kills the context and waits for outstanding
request references before freeing it. Only after that returns may coroutine-owned
buffers and descriptors be destroyed. Ring release is the safe fallback if the
graceful sequence reports an error.

If graceful destructor teardown fails, the abrupt ring-release fallback does not
resume any work still suspended at that point. Its guarantee is memory and
descriptor lifetime safety, not application-level completion notification.

## Operation Lifetime

The executor owns each `OperationState`. A submitted state, its coroutine handle,
and all referenced resources remain alive until one of these boundaries:

1. The operation CQE is consumed and the coroutine processes its result.
2. A cancellation CQE conclusively resolves the completion/cancellation race.
3. Abrupt executor teardown closes the ring and the kernel release path returns.

An operation is never freed merely because cancellation was requested.

## Buffer Lifetime

Async read, write, send, and receive APIs currently accept non-owning
`std::span`s. The caller must keep the span's backing storage alive and at a stable
address until the awaiting operation resumes or executor teardown completes.

Buffers local to a suspended coroutine frame satisfy this rule while the executor
retains that task. A span into temporary, moved, resized, or independently freed
storage does not.

Future owning-buffer APIs may relax this caller obligation, but non-owning APIs
will retain an explicit lifetime contract.

## Descriptor Lifetime

A descriptor remains open until every operation that names it is terminal.
`TcpStream`, `TcpListener`, and the awaiters they create share a small descriptor
state. Closing the public stream or listener releases that owner's reference;
the final in-flight awaiter closes the descriptor after its terminal result.

This ordering also prevents a late operation from acting on an unrelated resource
that reused the same numeric descriptor.

## Cancellation Contract

Cancellation is best-effort at the kernel request boundary and completion-guaranteed
at the runtime boundary: requesting cancellation does not itself make an operation
terminal, but the runtime continues processing until the original operation and
cancel request establish one final outcome.

The only public cancellation boundary currently implemented is the executor.
`shutdown()` rejects new tasks and operations, wakes blocking waits, prepares
cancel requests for pending operations, drains terminal CQEs, and lets affected
task chains unwind before destroying their state.

The cancellation machinery targets individual operation states internally, but
there is no public handle for cancelling one operation. There are also no public
task- or connection-cancellation APIs. Closing a stream or listener releases
descriptor ownership; it does not independently request cancellation of its
in-flight operations.

A parent coroutine owns an awaited child `Task` in its frame; the child's final
suspend resumes that parent. Spawned roots are owned by the executor until their
full awaited chain completes. Executor shutdown cancels a pending I/O leaf,
allowing child and parent frames to unwind in dependency order.

## Threading

All executor and executor-bound operations except `request_stop()` are confined to
the executor's owner thread. `request_stop()` is thread-safe: it sets an atomic stop
flag and writes an `eventfd` watched by io_uring, waking an owner blocked in
`run()`. External threads may not call other executor, listener, stream, or awaiter
methods.

## Implementation Status

Implemented:

- Stable executor address and operation-state ownership
- Completion-before-destruction during normal operation
- Ring-before-frame ordering during abrupt teardown
- Pending-operation destructor regression coverage
- Explicit executor shutdown that rejects new work
- Kernel cancellation and draining of original and cancellation CQEs
- Exactly-once resumption when completion races cancellation
- Deferred descriptor close while TCP operations remain in flight
- Pending receive, send, and accept shutdown with descriptor-reuse coverage
- Parent/awaited-child/root ownership through shutdown
- Cross-thread stop request using an io_uring-watched `eventfd`

Not yet implemented:

- Public per-operation, per-task, and per-connection cancellation

Deferred API work:

- Enforce buffer lifetime mechanically. The current span APIs are deliberately
  non-owning and therefore cannot detect destruction of caller storage. Replacing
  or supplementing them with owning/pinned buffer operations is an API-generation
  change; until then, violating the documented span lifetime is unsupported.

## Primary References

- [liburing `io_uring_queue_exit(3)`](https://man7.org/linux/man-pages/man3/io_uring_queue_exit.3.html)
- [Linux `io_uring_release()` and `io_ring_ctx_wait_and_kill()`](https://github.com/torvalds/linux/blob/master/io_uring/io_uring.c)
