# Lifetime and Cancellation Semantics

This document defines the ownership rules for executor, task, operation, buffer,
and descriptor lifetimes. It distinguishes the implemented abrupt teardown safety
path from the planned graceful cancellation path.

## Executor Lifetime

`IoUringExecutor::create()` returns a heap-owned, non-movable executor. Every
listener, stream, awaiter, and operation associated with an executor must be
destroyed before or as part of that executor's shutdown.

The destructor is the final safety boundary. It closes the io_uring instance
before destroying operation states or coroutine frames. Closing the ring invokes
the kernel ring-release path, which kills the context and waits for outstanding
request references before freeing it. Only after that returns may coroutine-owned
buffers and descriptors be destroyed.

Abrupt destructor teardown does not resume suspended coroutines and is not the
normal graceful-shutdown API. Its guarantee is memory and descriptor lifetime
safety, not application-level completion notification.

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

A descriptor must remain open until every operation that names it is terminal.
Closing a `TcpStream` or `TcpListener` while its operations remain pending is not
currently a supported graceful operation. The planned connection shutdown path
will request cancellation, resolve all completion races, and only then close the
descriptor.

This ordering also prevents a late operation from acting on an unrelated resource
that reused the same numeric descriptor.

## Cancellation Contract

Cancellation is best-effort at the kernel request boundary and completion-guaranteed
at the runtime boundary: requesting cancellation does not itself make an operation
terminal, but the runtime continues processing until the original operation and
cancel request establish one final outcome.

- **Individual operation:** target one operation state and resolve its original
  completion exactly once.
- **Task:** request cancellation of every operation owned by the task, then resume
  the task with a cancellation result after all targets are terminal.
- **Connection:** stop accepting new operations, cancel all operations using the
  descriptor, resolve their CQEs, then close the descriptor.
- **Executor:** reject new tasks and operations, wake blocking waits, cancel all
  pending operations, drain terminal CQEs, resolve tasks, destroy operation states
  and coroutine frames, then close the ring.

Structured parent/child task ownership is required before task cancellation can be
considered complete.

## Threading

All executor and executor-bound operations are confined to the executor's owner
thread. External threads may interact through ordinary OS resources, but they may
not call executor, listener, stream, or awaiter methods.

## Implementation Status

Implemented:

- Stable executor address and operation-state ownership
- Completion-before-destruction during normal operation
- Ring-before-frame ordering during abrupt teardown
- Pending-operation destructor regression coverage

Not yet implemented:

- Explicit `shutdown()` and stop-new-work state
- Async cancel SQEs and cancellation CQE routing
- Completion/cancellation race resolution
- Per-task and per-connection operation ownership
- Wakeup of a blocked executor wait
- Structured parent/child task ownership

## Primary References

- [liburing `io_uring_queue_exit(3)`](https://man7.org/linux/man-pages/man3/io_uring_queue_exit.3.html)
- [Linux `io_uring_release()` and `io_ring_ctx_wait_and_kill()`](https://github.com/torvalds/linux/blob/master/io_uring/io_uring.c)

