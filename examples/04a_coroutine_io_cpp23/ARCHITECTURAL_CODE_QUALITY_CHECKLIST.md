# Architectural Code Quality Checklist

**Project:** `io_uring_async_cpp23`  
**Purpose:** Evolve the C++23 example from a functional teaching prototype into a
correct, coherent, and credible reference architecture for coroutine-based
`io_uring` development.

## How to Use This Checklist

Work through phases in order. Later phases depend on the invariants established
by earlier phases. In particular, do not begin performance work until operation
ownership, cancellation, shutdown, and error propagation are correct and tested.

Each completed item should include or reference a test. A phase is complete only
when its exit criteria pass in debug, release, AddressSanitizer, and
UndefinedBehaviorSanitizer builds.

## Architectural Invariants

These invariants govern every phase:

- [x] Every submitted SQE has exactly one stable operation-state object.
- [x] An operation state remains alive until its CQE is consumed or cancellation
      is conclusively resolved.
- [ ] Buffers, socket addresses, file descriptors, and coroutine handles remain
      valid for the full lifetime required by the kernel.
- [x] Every submitted operation reaches exactly one terminal state: completed,
      cancelled, or drained during shutdown.
- [x] Executor destruction never leaves a coroutine or kernel operation referring
      to destroyed state.
- [x] Queue saturation and submission failures preserve their original error.
- [x] Root-task failures are observable by the caller.
- [x] Public APIs do not claim stronger behavior than they implement.

---

## Phase 0: Preserve the Known-Good Baseline

**Goal:** Make existing behavior reproducible before changing the runtime model.

- [x] Keep assertions enabled in release-mode tests.
- [x] Test that `spawn()` actually starts a lazy root task.
- [x] Retain spawned root tasks until completion.
- [x] Support a configurable echo-server port.
- [x] Return a failing process status when server startup fails.
- [x] Verify one real accept/receive/send echo round trip.
- [x] Split the current monolithic header test into focused test binaries:
  - [x] error tests
  - [x] task tests
  - [x] executor tests
  - [x] ring-wrapper tests
  - [x] TCP tests
  - [x] integration tests
- [x] Add the localhost echo round trip as an automated integration test using an
      ephemeral port.
- [x] Give debug, release, ASan, and UBSan separate build directories so objects
      compiled with incompatible flags can never be reused.
- [x] Add one command/target that runs the complete build-and-test matrix.

**Exit criteria:** A clean checkout can run the full matrix without manual port
selection, stale objects, or test-order dependencies.

---

## Phase 1: Define Operation Ownership and Completion Routing

**Goal:** Establish the lifetime model on which cancellation and safe shutdown
depend.

- [x] Write a short operation lifecycle specification covering:
  - [x] creation
  - [x] SQE preparation
  - [x] submission
  - [x] suspension
  - [x] CQE processing
  - [x] resumption
  - [x] destruction
- [x] Introduce a stable `OperationState` owned by the runtime for every submitted
      operation.
- [x] Store the awaiting coroutine handle and raw completion result directly in
      `OperationState`.
- [x] Put an `OperationState*` in SQE user data instead of routing through an
      operation-ID map and a separate result map.
- [x] Remove `pending_operations_`, `operation_results_`, and operation-ID lookup
      once direct operation-state routing is covered by tests.
- [x] Define how unknown, duplicate, and stale CQEs are handled; never silently
      discard them.
- [x] Prevent an executor from being moved after executor-bound objects or tasks
      can exist, or replace raw executor pointers with a stable runtime state.
- [x] Document whether the runtime is single-thread confined and enforce that
      contract in debug builds.

### Required tests

- [x] One completion resumes exactly one coroutine.
- [x] Multiple concurrent completions route to the correct coroutine.
- [x] Out-of-order completion stimuli produce the correct results.
- [x] Operation state remains valid across executor container growth.
- [x] Unknown or duplicate completion handling is deterministic and observable.
- [x] Executor movement cannot invalidate active streams, listeners, operations,
      or coroutine frames.

**Exit criteria:** Completion routing uses one stable state object per operation,
with no split ownership or result side channel.

---

## Phase 2: Correct Suspension and Error Propagation

**Goal:** Make every failure path truthful, non-reentrant, and easy to reason about.

- [x] Change I/O awaiters so SQE-preparation failure does not manually resume the
      awaiting coroutine from inside a `void await_suspend`.
- [x] Use the `bool` or coroutine-handle form of `await_suspend` to express whether
      suspension actually occurred.
- [x] Preserve the exact `Error` returned by SQE preparation; do not translate all
      preparation failures to `ENOMEM` or later surface them as `EINVAL`.
- [x] Remove the unused `raw_result_` field or make it the single authoritative
      completion result.
- [x] Make `get_sqe()` report queue saturation distinctly from allocation failure.
- [x] Replace assertion-only task preconditions with defined release behavior where
      misuse can occur through the public API.
- [x] Define how exceptions from awaited tasks propagate.
- [x] Define how exceptions from detached/root tasks are reported to `run()` or an
      executor error handler.
- [x] Make executor submission failures resolve or cancel all affected operations;
      never return while suspended coroutines remain stranded.

### Required tests

- [x] SQ full before preparation.
- [x] `io_uring_submit()` failure.
- [x] Negative CQE result for every operation category.
- [x] Exception from a nested task.
- [x] Exception from a spawned root task.
- [x] No manual synchronous reentrant resume on preparation failure.

**Exit criteria:** Every error retains its cause, is observable at the correct API
boundary, and leaves no suspended or leaked work.

---

## Phase 3: Cancellation, Shutdown, and Resource Lifetime

**Goal:** Make destruction and shutdown safe under in-flight I/O.

**Status:** Complete for the documented non-owning-buffer API. Mechanical buffer
lifetime enforcement is explicitly deferred to an owning/pinned-buffer API cycle.

- [x] Define cancellation semantics for:
  - [x] individual operations
  - [x] a task and its child operations
  - [x] a connection
  - [x] the entire executor
- [x] Implement cancellation using `IORING_OP_ASYNC_CANCEL` where supported.
- [x] Handle completion/cancellation races without double resume or double free.
- [x] Decide and document whether cancellation is best-effort or completion-guaranteed.
- [x] Prevent `TcpStream` and `TcpListener` from synchronously closing a descriptor
      while an operation still references it.
- [x] Protect against descriptor reuse causing a late operation to affect a new
      resource.
- [x] Define executor destruction as an explicit sequence: stop accepting work,
      request cancellation, drain CQEs, destroy operation states, then destroy the
      ring and coroutine frames.
- [x] Add a wake mechanism so shutdown can interrupt a pending accept or wait.
- [x] Define buffer ownership requirements for every async API.
- [x] Resolve non-owning-span enforcement: deferred to an owning/pinned-buffer API;
      the current API explicitly makes backing-storage lifetime a caller contract.
- [x] Add structured task ownership so parent/child shutdown is deterministic.

### Required tests

- [x] Cancel before submission.
- [x] Cancel after submission but before completion.
- [x] Completion wins a cancellation race.
- [x] Cancellation wins a completion race.
- [x] Close a stream with pending send and receive operations.
- [x] Shut down with a pending accept.
- [x] Destroy an executor with pending operations.
- [x] Reuse the numeric value of a recently closed descriptor.
- [x] Run cancellation and shutdown tests repeatedly under ASan and UBSan.

**Exit criteria:** No supported shutdown or cancellation sequence can cause a
use-after-free, double resume, stranded coroutine, or descriptor-lifetime race.

---

## Phase 4: Make I/O Semantics Complete and Honest

**Goal:** Ensure high-level APIs match real stream and kernel behavior.

**Status:** Complete. Primitive and composed transfer semantics, EOF/error
mapping, accepted-socket flags, IPv4/IPv6 behavior, and close ownership are
documented and covered by live-kernel tests.

- [x] Pass send and receive flags through every layer or remove the unused flags
      parameters.
- [x] Add `send_all()` that correctly handles partial sends.
- [x] State clearly that `send()` may complete after sending fewer bytes than
      requested.
- [x] Decide how interrupted, would-block, and connection-closed results map into
      the public error model.
- [x] Query `getsockname()` after binding port zero so `TcpListener::port()` returns
      the actual assigned port.
- [x] Validate backlog and descriptor arguments at public boundaries.
- [x] Define EOF semantics consistently for files and streams.
- [x] Add async close only if its ownership contract is clearer than synchronous
      RAII close; otherwise remove `CloseAwaitable` from the public surface.
- [x] Decide whether accepted sockets inherit required flags and test them.
- [x] Ensure high-level TCP sends cannot terminate the process through `SIGPIPE`.
- [x] Document that `send_all()` may fail after transmitting a prefix.
- [x] Add IPv6/address-family support only after the IPv4 contract is complete.

### Required tests

- [x] Forced partial send with eventual full delivery.
- [x] Peer closes normally during receive.
- [x] Peer resets during send and receive.
- [x] Bind to port zero and verify the reported port accepts connections.
- [x] Invalid descriptor and invalid argument cases.
- [x] Large payload echo spanning many buffer and CQE cycles.

**Exit criteria:** Each API documents and implements the exact semantics callers
need to write correct applications.

---

## Phase 5: Simplify Modules and Remove Duplication

**Goal:** Apply KISS and DRY after correctness has established the right
abstractions.

**Status:** Complete. Stable submission, completion, and scheduling boundaries
are consolidated; strict example parsing has direct unit coverage.

- [x] Introduce one private SQE-preparation helper that centralizes:
  - [x] SQE acquisition
  - [x] queue-full handling
  - [x] user-data attachment
- [x] Introduce shared completion-result conversion for byte-count operations.
- [x] Keep operation-specific policy visible; do not hide important io_uring
      behavior behind generic metaprogramming.
- [x] Remove unused fields, includes, forward declarations, and stale comments.
- [x] Reduce public exposure of raw `io_uring` access; document the invariants a
      caller must preserve if raw access remains available.
- [x] Consolidate `run()` and `run_once()` completion-draining logic.
- [x] Separate executor scheduling policy from ring/completion mechanics if both
      continue to grow. Scheduling remains in the public loop methods; shared
      submission and CQE mechanics now live in focused private helpers.
- [x] Use `std::from_chars` for strict, allocation-free example port parsing.
- [x] Keep source files focused enough that ownership and suspension behavior can
      be understood without cross-referencing several unrelated modules.

**Exit criteria:** Duplication is removed at stable abstraction boundaries, with
no loss of clarity about submission, suspension, ownership, or completion.

---

## Phase 6: Testing as an Architectural Contract

**Goal:** Make regressions in coroutine and kernel-facing behavior difficult to
introduce unnoticed.

**Status:** Complete for the current supported host. Minimum-kernel CI is
deferred until Phase 7 establishes the supported kernel and liburing baseline.

- [x] Replace the hand-written `TEST`/`PASS` counting macros or ensure failures do
      not depend solely on `assert`.
- [x] Separate kernel-independent unit tests from host-kernel integration tests.
- [x] Report a clear skip when `io_uring` is blocked by seccomp or unavailable,
      while still failing genuine library regressions.
- [x] Add deterministic test seams for submission and completion without requiring
      every edge case from the live kernel.
- [x] Add integration tests for read, write, accept, send, receive, close, partial
      I/O, cancellation, and shutdown.
- [x] Add stress tests with many concurrent operations and randomized completion
      order.
- [x] Add compile-time API checks for move-only ownership and invalid operations.
- [x] Add leak detection for coroutine frames, operation states, sockets, and ring
      resources.
- [x] Add ThreadSanitizer only if cross-thread submission or execution becomes a
      supported feature. Not applicable while execution remains owner-thread
      confined; the supported cross-thread stop request has integration coverage.
- [ ] Run tests on the minimum supported kernel as well as the current development
      kernel. **Deferred to Phase 7:** the minimum version is not yet defined.

**Exit criteria:** Tests exercise documented invariants and failure paths rather
than merely proving that headers compile and happy paths work.

---

## Phase 7: Feature Detection and Portability Policy

**Goal:** Make kernel and liburing requirements accurate and observable.

- [ ] Establish and document the minimum supported Linux kernel, liburing, and GCC
      versions.
- [ ] Probe required opcodes and optional features at startup.
- [ ] Distinguish unsupported kernel functionality from permission/seccomp denial.
- [ ] Provide explicit fallbacks only when they preserve the documented semantics.
- [ ] Add feature gates for optional setup flags and modern operations.
- [ ] Test degraded operation on the minimum supported feature set.
- [ ] Avoid using host-specific `-march=native` in distributable release builds;
      reserve it for an explicit native/benchmark target.

**Exit criteria:** Users can determine before serving traffic whether the runtime
and requested features are supported on their host.

---

## Phase 8: Performance Work

**Goal:** Optimize measured bottlenecks without weakening correctness.

**Status:** Deferred to a future development cycle. Performance work remains
blocked until the correctness, lifecycle, testing, and portability gates in
Phases 0 through 7 are complete.

- [ ] Establish reproducible latency, throughput, CPU, syscall, and allocation
      baselines.
- [ ] Measure the direct operation-state design before adding batching complexity.
- [ ] Add batching policy with explicit submission thresholds and latency bounds.
- [ ] Evaluate `IORING_SETUP_SINGLE_ISSUER`, `IORING_SETUP_COOP_TASKRUN`, and
      `IORING_SETUP_DEFER_TASKRUN` behind feature detection.
- [ ] Evaluate multishot accept only after cancellation and shutdown are reliable.
- [ ] Evaluate provided buffers/buffer rings only after buffer ownership is explicit.
- [ ] Evaluate registered files and buffers with clear registration lifetimes.
- [ ] Add performance regression thresholds that are stable enough for CI.
- [ ] Document benchmark hardware, kernel, liburing version, compiler flags, and
      workload.

**Exit criteria:** Every performance claim is supported by a reproducible benchmark
and does not bypass lifecycle or error-handling invariants.

---

## Phase 9: Documentation and Canonical Project Status

**Goal:** Make the repository a trustworthy learning and reference resource.

- [ ] Designate the C++23 implementation as canonical, or port all correctness fixes
      and tests back to the C++20 implementation.
- [ ] If C++20 remains historical, label it clearly and prevent users from mistaking
      it for the maintained implementation.
- [ ] Update feature claims so zero-copy, batching, concurrency, cancellation, and
      performance are described as implemented only when verified.
- [ ] Document coroutine-frame, buffer, executor, descriptor, and operation-state
      ownership with lifecycle diagrams.
- [ ] Document error propagation and root-task failure behavior.
- [ ] Document shutdown and cancellation guarantees.
- [ ] Provide one minimal example per public abstraction and one production-shaped
      server example.
- [ ] Add a limitations section that is maintained alongside feature development.
- [ ] Keep the README, design documents, checklist, and public headers consistent.

**Exit criteria:** A reader can understand what is implemented, what is guaranteed,
what remains experimental, and how every important resource is owned.

---

## Definition of “Norm-Setting”

The project may describe itself as a norm-setting io_uring reference only when:

- [ ] Phases 0 through 7 are complete.
- [ ] All architectural invariants are enforced and tested.
- [ ] A real echo integration suite covers concurrency, partial I/O, cancellation,
      and graceful shutdown.
- [ ] Sanitizers find no leaks or invalid lifetime behavior under stress.
- [ ] Supported kernel and liburing versions are tested and documented.
- [ ] Performance claims are reproducible and qualified.
- [ ] Documentation describes actual behavior without aspirational claims presented
      as current features.
