# Operation Lifecycle

This document defines the implemented ownership and state-transition contract for
one asynchronous io_uring operation. It is normative for the C++23 runtime.

## Ownership Model

Each submitted SQE has exactly one stable `OperationState`. The executor owns that
state from creation until the corresponding CQE has been consumed and any awaiting
coroutine has been resumed. SQE user data identifies the state directly.

The awaiting coroutine owns the high-level awaiter. The awaiter may reference a
buffer or socket, but those resources must remain valid until the operation reaches
a terminal state. Destroying a task does not by itself prove that the kernel has
stopped accessing its operation state or buffers.

## State Transitions

```text
created
   |
   v
prepared ---- preparation failed ----> failed
   |
   v
submitted ---- cancellation requested ----+
   |                                       |
   +---- CQE consumed ---------------------+
                    |
                    v
          completed or cancelled
                    |
                    v
                 resumed
                    |
                    v
                 destroyed
```

`failed`, `completed`, and `cancelled` are terminal operation outcomes. A failed
preparation is never submitted and therefore needs no CQE. A submitted operation
is not terminal until its completion or cancellation CQE has been consumed.

## Creation

The awaiter asks the executor to create an operation state. The state contains the
awaiting coroutine handle, raw result storage, lifecycle status, and cancellation
bookkeeping. Its address must remain stable through CQE consumption.

## SQE Preparation

The operation obtains one SQE, prepares the opcode-specific fields, and attaches
its operation-state address as user data. Preparation failure preserves the exact
error and returns without suspending or registering pending kernel work.

## Submission

After successful preparation, the executor owns the pending operation. Submission
may be batched, but each prepared operation must be accounted for. If submission
fails, every affected operation is resolved with that failure or retained for a
documented retry; no coroutine may remain silently stranded.

## Suspension

The coroutine suspends only after its operation has been prepared and registered.
Preparation failure uses the boolean or coroutine-handle `await_suspend` contract
to continue without reentrant manual `resume()` calls.

## CQE Processing

The executor reads the operation-state address from CQE user data, validates that
the state is pending, records the raw CQE result exactly once, and marks the CQE as
seen. Unknown, duplicate, or stale completions are runtime errors and must be
observable; they are never silently ignored.

## Resumption

After recording the result, the executor resumes the awaiting coroutine exactly
once. `await_resume()` converts the stored kernel result into the documented
`std::expected` value. Root-task exceptions are reported through the executor's
error channel rather than discarded.

## Cancellation

Cancellation is a competing asynchronous operation, not immediate destruction.
The original operation state, coroutine frame, descriptor, and buffer remain alive
until the runtime consumes the CQE that conclusively resolves the
completion/cancellation race.

## Destruction

An operation state may be destroyed only after its terminal CQE has been consumed
and resumption bookkeeping is complete. The exception is abrupt executor teardown:
the executor first closes the ring and waits for the kernel release path, then
destroys operation states and coroutine frames. This emergency path guarantees
resource lifetime safety but does not resume suspended coroutines.

Graceful executor shutdown stops new work, requests cancellation, drains terminal
CQEs, resumes or resolves affected tasks, destroys operation states and coroutine
frames, and only then tears down the ring. Its full contract is specified in
`LIFETIME_SEMANTICS.md`.

## Threading Contract

The current runtime is single-thread confined and heap-owned so its address remains
stable for executor-bound objects. Creation, spawning, SQE preparation,
submission, CQE processing, and task resumption must occur on the executor's owner
thread. External threads may use ordinary sockets in integration tests, but may not
call executor or executor-bound APIs. Debug builds enforce this contract.

Exceptions from awaited tasks propagate through `await_resume()`. Exceptions from
spawned root tasks are observed by the executor and returned from `run()` or
`run_once()` as `Error::Code::TaskFailed`.
