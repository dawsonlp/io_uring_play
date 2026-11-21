# Proposal: C++26 Completion-Oriented Execution

**Repository:** `dawsonlp/io_uring_play`  
**Status:** Initial continuation and file-read prototypes verified; ownership and composition evaluation pending\
**Experiment:** `examples/05_execution_cpp26/`  
**Date:** 2026-11-20

## Vision

Investigate whether a runtime organized around **operations, outcomes, and explicit resource ownership** produces a cleaner programming model than readiness-oriented event loops and channel/handler frameworks. The architectural benefit of clear completion semantics matters independently of throughput.

Linux `io_uring` supplies an actual completion-oriented I/O mechanism. C++26 `std::execution` (senders, receivers, operation states and schedulers) offers a standard vocabulary for asynchronous composition. We will test how naturally those layers fit, rather than assume either is the answer.

This is a learning and architecture experiment, **not** a new production networking framework, a claim of superior performance, or a commitment to build a reusable library.

## Existing work and relationship to this experiment

This repository already includes:

- `01_basic_file_read`: a C SQE/CQE demonstration.
- `02_basic_file_read_cpp`: an introductory C++ RAII version.
- `03_async_echo_server`: early design exploration.
- `04_coroutine_io`: an experimental C++20 coroutine runtime.
- **`04a_coroutine_io_cpp23`**: a correctness-focused, tested C++23 runtime with stable operation states, CQE routing, file/TCP operations, cancellation during executor shutdown, descriptor lifetime control, and unit, integration, sanitizer and stress tests.

The C++23 implementation is the comparison baseline—not disposable scaffolding. Its normative `IO_SEMANTICS.md`, `LIFETIME_SEMANTICS.md`, `OPERATION_LIFECYCLE.md`, and `ARCHITECTURAL_CODE_QUALITY_CHECKLIST.md` should guide review of the new approach. Known gaps include mechanically enforced buffer ownership, public per-operation cancellation, capability probing, and reproducible performance baselines.

Create `examples/05_execution_cpp26/` as a **parallel, independently buildable** experiment. Do not migrate or rewrite earlier examples to adopt it. Only designate a canonical implementation after comparative evidence.

## Development and execution environments

The **coordination machine is a personal MacBook Air running macOS**, not the target Linux host. The proposal and repository can be edited from that machine, but Linux `io_uring` examples **cannot be built and exercised there as Linux tests**.

The target machine is a separate Dell Precision 5570 running Ubuntu 26.10, x86-64, with Linux 7.3.x, GCC 16.2, Clang 22.1.8, and `liburing-dev` 2.15 installed. A **Codex agent working on the Linux laptop**, with explicit access to its checkout and toolchain, will perform later implementation, compilation, and test runs. Repository changes made from the Mac are not evidence of successful builds or runtime behavior on Linux.

Initial intended stack:

| Concern | Choice |
| --- | --- |
| Language | C++26 mode |
| Execution abstractions | C++26 `std::execution`, initially via pinned NVIDIA `stdexec` |
| I/O | Linux `io_uring`; `liburing` for baseline/direct comparisons |
| Build description | Meson |
| Build executor | Ninja |
| Compilers | GCC 16 primary; Clang 22 secondary |
| Verification | On the Ubuntu host, not the Mac |

Do not presume the standard-library `std::execution` implementation is complete merely because the compiler accepts `-std=c++26`. Distinguish C++26-standard interfaces from `stdexec` extensions, including its experimental `io_uring_context`. Pin dependencies when implementation starts; verify actual feature and permission support on the Linux target.

Meson/Ninja is an experiment-local choice, not a repository-wide Makefile migration.

## Core hypotheses

1. A completion-oriented public abstraction can expose fewer concepts than an event-oriented networking framework.
2. Senders/receivers can express kernel I/O outcomes, cancellation, and composition without adding undue conceptual or implementation complexity.
3. C++ coroutines can offer sequential syntax over the same underlying operations; they should not require a second, competing execution model.
4. Ownership of buffers, descriptors, operation state and coroutine state can be made explicit and, where feasible, mechanically enforced.
5. Files, sockets and timers may share execution/composition primitives while retaining distinct error, progress, and cancellation semantics.

These are questions to test, not conclusions.

## Design commitments

- **Operations and outcomes first.** Applications describe work and observe outcomes; SQEs, CQEs, readiness notification and event loops should not leak into ordinary application APIs without demonstrated need.
- **Mechanism versus policy.** Distinguish ring submission, completion dispatch, continuation scheduling, admission limits and buffer policy.
- **Reality before abstraction.** Preserve kernel semantics, including short transfers, EOF, asynchronous errors, multishot completions, and cancellation races.
- **Use standard vocabulary before inventing another framework.** Evaluate `std::execution` and coroutines directly; introduce new DSL elements only to solve observed problems.
- **Simplicity before generality.** Judge complexity by states, ownership relations, synchronization and programmer-visible concepts, not just line count.
- **Correctness before performance.** Benchmark only after comparable, verifiably correct workloads exist.
- **Reversibility.** An experiment may demonstrate that direct `liburing`, the existing C++23 coroutine model, or a hybrid is preferable.

## Invariants to preserve and test

1. Each submitted **single-shot** operation has a stable, authoritative state and exactly one terminal result delivery. Multishot requests have an explicit series of completions and a separate terminal condition.
2. Every buffer, descriptor, address, registration and coroutine frame referenced by the kernel remains valid until the kernel no longer needs it.
3. Cancellation requests do not prove completion or permission to release resources; cancellation and successful completion may race.
4. Queue-full, submission, I/O, cancellation and shutdown errors remain visible and accurately classified.
5. Completion dispatch must not resume a destroyed computation or resume it twice.
6. In-flight operations and retained memory are bounded by explicit admission/resource policies.
7. Safe shutdown stops admission, reconciles/cancels work, drains required completions and only then releases dependent resources.
8. High-level convenience operations must not pretend an individual read/write always transfers everything requested.
9. Unsupported kernel operations and security-policy denials are distinguished from successful feature detection.
10. Public claims should match tests and documented semantics, not design intent.

The existing C++23 test suite and design documents are references; passing them in that implementation does not prove the new implementation correct.

## Continuation contract for the single-shot prototype

- One owner thread starts I/O operations and drives the ring. An operation may complete inline during `start()` if it is rejected or stopped before submission. Submitted operations deliver their result on the ring owner thread. Downstream work runs there unless the composition explicitly requests a scheduler hop. A stop request from another thread must reach the owner without directly invoking the receiver.
- Before invoking the receiver, copy the CQE result, retire the CQE and its dispatch bookkeeping, and release any internal lock. Keep any resources needed by the delivered result alive across the handoff. Receiver code may immediately start another operation or destroy the completed operation state. The dispatch and inline paths must therefore finish using that state before invoking the receiver.
- An honored stop before submission or confirmed cancellation of the target by the runtime's stop machinery yields `set_stopped`. A successful target CQE yields `set_value` with the actual byte count, even if stop was requested. A zero-byte read is a value, interpreted according to the read's semantics. Submission and I/O failures yield `set_error`.
- The cancellation request and its target have separate CQE identities. A cancellation CQE, including `-ENOENT` or `-EALREADY`, does not replace the target CQE. Retain kernel-referenced resources through target completion and keep any outstanding cancellation bookkeeping valid independently of the receiver. Reserve the target identity until the target CQE and any outstanding cancel CQE have been retired, so a delayed cancel cannot match a new operation. During graceful shutdown, stop admission and drive both kinds of CQEs until outstanding work has drained.

## Experimental sequence and exit criteria

### Phase 0 — Baseline and host verification

On the Ubuntu target, capture installed compiler, `liburing`, kernel, and build-tool versions; verify unprivileged ring creation. Run the existing `04a_coroutine_io_cpp23` debug/release/ASan/UBSan matrix and record results. Do not alter the baseline to accommodate the new experiment.

**Exit:** Reproducible baseline and a verified Linux execution environment.

### Phase 1 — Understand execution without I/O

Pin `stdexec` and build minimal senders/receivers covering value, error, stopped, scheduling, cancellation, composition, and (where supported) coroutine interoperability. Compile with GCC and Clang. Record which APIs are standardized and which are extensions.

Exercise synchronous completion with a receiver that destroys the owning operation state. Record the thread used by inline and scheduled continuations. Use the pinned revision's timer example to check the experimental scheduler separately from file I/O.

**Exit:** A tested explanation of the execution lifecycle independent of `io_uring`.

### Phase 2 — One actual completion

Implement a known-data file-read sender over `liburing`. Compare with a direct `liburing` read and exercise the experimental `stdexec` context for the same read if the pinned revision exposes an appropriate API. Check byte counts, EOF and an error path. Test receiver-driven destruction and immediate submission of a follow-up read during CQE dispatch.

**Exit:** Verified data, observable failures, and a documented operation/resource lifecycle.

### Phase 3 — Ownership and failure behavior

Test stable operation state, borrowed versus owning buffers, descriptor reuse, SQ exhaustion, failed submissions, cancellation races, coroutine destruction, and executor shutdown. Run deterministic tests and sanitizer-enabled Linux tests.

Drive both CQE arrival orders for stop races, including a successful target completion with cancellation still outstanding. Assert one receiver completion, retirement of both CQE identities, and resource release at the documented boundary.

**Exit:** No known unsupported lifetime claim; critical state transitions covered by tests.

### Phase 4 — Composition

Compare two dependent reads and a bounded pair of concurrent reads against C++23 using equivalent buffer and cancellation contracts. Exercise both sender composition and sequential coroutine syntax over the same I/O sender. Record caller ownership obligations, allocations, scheduler hops and the code needed to propagate failure and stop. Identify any stronger guarantees separately so the comparison accounts for their implementation cost.

**Exit:** Record whether sender/receiver composition makes either workload simpler while preserving observable outcomes and lifetimes. Use that evidence to decide whether to continue to TCP and streaming.

### Phase 5 — Network and streaming

Implement a minimal concurrent TCP echo path with accept, receive, send, partial progress, disconnect, and controlled shutdown. Then investigate bounded socket-to-file streaming to expose backpressure and buffer ownership requirements.

**Exit:** Comparable workload and explicit resource controls; still no premature HTTP/TLS framework.

### Phase 6 — Optional advanced kernel capabilities

Only after correctness is established, consider multishot accept/receive, provided buffer rings, registered resources, batching, and zero-copy facilities. Probe actual runtime support and avoid equating registered buffers or provided buffers with guaranteed zero-copy execution.

**Exit:** Each optional feature has measured benefits or a documented reason to defer.

### Phase 7 — Architectural evaluation

Compare direct `liburing`, `04a_coroutine_io_cpp23`, and the C++26 sender/receiver experiment for correctness, expressive economy, ownership guarantees, code complexity, CPU/allocations, latency and throughput under equivalent workloads.

**Exit:** Written decision: adopt, adapt, combine, or reject the proposed model, with evidence and limitations.

## Initial deliverables

- This proposal and a short `examples/05_execution_cpp26/README.md`.
- Later: pinned build dependencies, Meson configuration, minimal execution examples, lifecycle specifications, tests, and comparison notes.
- Eventually: a defensible architectural assessment, **not necessarily a new framework**.

## Non-goals

No production HTTP stack, TLS implementation, cross-platform runtime, actor framework, benchmark-backed performance claim, broad repository build-system conversion, or premature custom execution DSL.

## Definition of success

We should be able to explain, from observations and tests, whether the C++26 execution machinery makes completion-oriented I/O **simpler**, where ownership and cancellation constraints must live, what it costs, and whether it deserves to be the foundation for future systems.

A negative result is useful if it exposes the actual constraints and leads to a better model.

**Guiding idea:** Applications express desired operations; the runtime coordinates execution and lifetimes; Linux performs eligible I/O; completions report outcomes.
