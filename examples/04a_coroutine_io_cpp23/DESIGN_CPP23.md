# C++23 Runtime Design

## Scope

This example explores a small coroutine runtime directly on top of `liburing`.
The design prioritizes explicit ownership, observable errors, and understandable
state transitions. It is not a general asynchronous framework and does not claim
production readiness or measured performance.

C++23 is used where it improves the API, principally through `std::expected` and
`std::print`. The runtime does not depend on proposed or compiler-specific
coroutine extensions.

## Architecture

### `IoUring`

`IoUring` is a move-only RAII wrapper around one initialized `io_uring`. It owns
ring setup and teardown and exposes typed preparation methods for the operations
used by this example. SQE acquisition, queue-full reporting, and user-data
attachment are centralized in a private helper.

The raw ring and raw SQE acquisition are not public extension points. Supporting
arbitrary operations would require an API that preserves the executor's ownership
and completion-routing invariants.

### `IoUringExecutor`

The executor is created through `IoUringExecutor::create()` and returned as a
`std::unique_ptr`. Its address is stable and the type cannot be copied or moved.

The executor owns:

- spawned root tasks;
- the ready-task queue;
- every live `OperationState`;
- internal cancellation states;
- the ring and shutdown wake `eventfd`.

Normal executor methods are confined to the creating thread. `request_stop()` is
the sole cross-thread method: it sets an atomic flag and writes the wake fd.

`run()` processes ready work and blocks for completions until user work is gone.
`run_once()` processes the ready work present at entry and drains currently
available completions without blocking. Shared submission and CQE mechanics are
private helpers; scheduling policy remains visible in the two loop methods.

### `OperationState`

Each prepared operation has one stable executor-owned state. Its address is
stored in SQE user data. A state records the continuation, raw CQE result, error,
status, and cancellation bookkeeping.

There is no operation-ID/result side channel. Unknown, duplicate, and stale CQEs
are errors rather than silently ignored events.

### Awaitables

Concrete awaitables prepare file read/write and socket accept/send/receive
operations. SQE-preparation failure uses the boolean `await_suspend` contract so
the coroutine continues without a reentrant manual resume. `await_resume()`
obtains the state result and converts negative kernel results into `Error`.

Read, write, send, and receive share byte-count conversion. Accept retains its
descriptor-specific result policy.

Awaitables borrow their buffers through `std::span`. The runtime does not pin,
copy, register, or otherwise extend backing-storage lifetime.

### Tasks

`Task<T>` is lazy. A root task begins when it is spawned and remains owned by the
executor until completion. An awaited child task remains owned through the parent
coroutine frame. Child exceptions propagate through `await_resume`; root-task
exceptions become `Error::Code::TaskFailed` at the executor boundary.

The runtime provides structured lifetime through parent/child ownership, but it
does not provide a public task group, join handle, or task-cancellation token.

### TCP ownership

`TcpListener` and `TcpStream` are move-only RAII types. They share descriptor
state with their in-flight awaiters. Calling `close()` releases the public
owner's reference; the numeric descriptor closes only after the last in-flight
operation releases its reference.

This is synchronous ownership release, not asynchronous close. There is no
`CloseAwaitable`.

## Shutdown and cancellation

The implemented public cancellation boundary is executor-wide shutdown.
Shutdown stops new work, prepares async-cancel requests for pending operations,
submits and drains completions, and lets task chains unwind. A completion may win
a race with its cancellation; either way, the original operation resumes once.

Destructor teardown first attempts the graceful path. If that path fails, closing
the ring is the final lifetime boundary before remaining states and frames are
destroyed. The fallback protects memory and descriptor lifetime but does not
promise application-level resumption for work still suspended at that point.

There are no public per-operation, per-task, or per-connection cancellation APIs.

## I/O behavior

- Primitive file and socket transfers may be partial.
- File read and TCP receive report successful zero bytes as EOF/orderly shutdown.
- `send_all()` loops until the buffer is accepted or an error occurs; an error may
  follow partial progress that is not included in the current error value.
- TCP send defaults to `MSG_NOSIGNAL`.
- Accepted sockets are explicitly nonblocking and close-on-exec.
- IPv6 listeners use `IPV6_V6ONLY`.
- Binding port zero records the kernel-assigned port.

The exact contract and errno mapping are specified in
[IO_SEMANTICS.md](IO_SEMANTICS.md).

## Error model

Public fallible operations use `std::expected<T, Error>`. Important boundaries
preserve their original cause:

- SQ exhaustion is `QueueFull`.
- Submission failure is propagated to affected operations.
- Negative CQE results map from their kernel errno.
- Root-task exceptions become `TaskFailed`.
- Invalid public arguments are rejected before submission where practical.

The error model does not currently carry partial-progress metadata for composed
operations.

## Testing model

Kernel-independent tests cover tasks, errors, API traits, parsing, and scripted
operation/completion state. Live integration tests cover the ring, regular files,
TCP, partial I/O, completion routing, cancellation, shutdown, and descriptor
lifetime.

Integration tests use exit status 77 when ring creation is unavailable because of
the host kernel or execution policy. Release, debug, ASan, and UBSan builds use
separate output trees. ASan enables leak detection; Linux descriptor-count tests
cover non-memory resources.

## Deliberate exclusions

The following are not current capabilities:

- cross-thread submission or execution;
- owning or registered buffers;
- registered files;
- multishot operations;
- zero-copy send;
- timers and timeouts;
- TLS;
- feature probing and fallbacks;
- a stable ABI or compatibility guarantee;
- benchmark-backed performance claims.

These may be evaluated only through the unchecked phases of the architectural
checklist. Their presence in the roadmap does not imply implementation.

## Portability status

The implementation is Linux-only and links directly to `liburing`. A minimum
kernel, `liburing`, and compiler version has not been declared. The README records
the most recently verified local environment; it must not be interpreted as a
support matrix.
