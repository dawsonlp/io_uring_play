# io_uring Kernel Feature Matrix

This document separates three things that are often conflated:

1. the Linux kernel release that introduced an io_uring capability;
2. the liburing helper that makes that capability convenient to use; and
3. the features actually used by this repository.

The kernel has supported io_uring since Linux 5.1. The table below is a
version-by-version milestone guide for the mainline kernel releases that have
introduced capabilities relevant to this project or to its roadmap. A row with
“no new milestone” still inherits all earlier capabilities. This is a guide,
not a support declaration: distributions backport features, and the runtime
must probe optional operations and ring features at startup.

## Kernel release to feature matrix

| Kernel | io_uring milestone(s) relevant to this project and roadmap |
| --- | --- |
| 5.1 | Initial io_uring API: shared SQ/CQ rings, SQE/CQE submission and completion, basic file and socket operations, cancellation, and timeouts. |
| 5.2 | Early ABI and enter/wait refinements; retain runtime probing rather than assuming later flags. |
| 5.3 | Early fixed-resource and request-linking capabilities; availability varies by operation and kernel configuration. |
| 5.4 | SQ and CQ can share one mmap region; older kernels require separate mappings. liburing hides this setup detail. |
| 5.5 | No new project-critical milestone recorded here; inherits the 5.1–5.4 interface. |
| 5.6 | No new project-critical milestone recorded here. |
| 5.7 | Provided-buffer operations become available, enabling buffer selection for receive-style operations. |
| 5.8 | No new project-critical milestone recorded here. |
| 5.9 | No new project-critical milestone recorded here. |
| 5.10 | Registered restrictions and extended-argument enter support; these enable stronger opcode policy and more efficient timed waits on supporting kernels. |
| 5.11 | No new project-critical milestone recorded here. |
| 5.12 | No new project-critical milestone recorded here. |
| 5.13 | Registered-resource and buffer-registration behavior expands; registration lifetimes become increasingly important to correctness. |
| 5.14 | No new project-critical milestone recorded here. |
| 5.15 | No new project-critical milestone recorded here. |
| 5.16 | No new project-critical milestone recorded here. |
| 5.17 | No new project-critical milestone recorded here. |
| 5.18 | No new project-critical milestone recorded here. |
| 5.19 | Multishot accept and ring-mapped provided-buffer rings become available; one request may produce multiple CQEs and must be drained using `IORING_CQE_F_MORE`. |
| 6.0 | Zero-copy send operations become available on supporting kernels and network paths. |
| 6.1 | Deferred task running and additional multishot/poll improvements provide executor-tuning opportunities, subject to feature checks. |
| 6.2 | No new project-critical milestone recorded here. |
| 6.3 | Registered ring-file-descriptor support reduces enter overhead for selected designs. |
| 6.4 | No new project-critical milestone recorded here. |
| 6.5 | No new project-critical milestone recorded here. |
| 6.6 | No new project-critical milestone recorded here. |
| 6.7 | Multishot read support expands streaming use cases, normally with provided buffers. |
| 6.8 | No new project-critical milestone recorded here. |
| 6.9 | No new project-critical milestone recorded here. |
| 6.10 | No new project-critical milestone recorded here. |
| 6.11 | Minimum-timeout waiting support is available on kernels advertising `IORING_FEAT_MIN_TIMEOUT`; liburing reports unsupported use as `-EINVAL`. |
| 6.12 | Minimum-timeout and bundled receive/send capabilities are documented for newer kernels; provided-buffer and CQE-flag handling become more important. |
| 6.13 | No new project-critical milestone recorded here. |
| 6.14 | No new project-critical milestone recorded here. |
| 6.15 | Zero-copy receive development continues; require explicit kernel and NIC capability checks before treating it as available. |
| 6.16 | No new project-critical milestone recorded here. |
| 6.17 | io_uring zero-copy receive is documented as a kernel feature with NIC requirements, `IORING_SETUP_SINGLE_ISSUER`, and explicit userspace buffer-area setup. |
| 7.0 | cBPF io_uring filters and task-inherited restrictions are merged; these enable kernel-enforced opcode policy but require explicit policy and permission handling. |
| 7.1 | No additional project-critical milestone recorded here; inherits 7.0 features. |
| 7.2 | Mainline release as of 2026-08-16; no additional project-critical io_uring milestone has been added to this matrix yet. |
| 7.3 (development placeholder) | Not released. Track linux-next and release-candidate changes provisionally; do not use this row for support claims until 7.3 is released and the final UAPI and io_uring documentation are available. |

The “no new project-critical milestone” entries are intentional. They prevent
the matrix from implying that every kernel release changed the public io_uring
ABI while still making the supported release range easy to scan.

## Features used by this repository

This mapping describes the actively developed C++23 runtime in
`examples/04a_coroutine_io_cpp23`, not every exploratory document in the tree.

| Project feature | Kernel/API surface | Used where | Status and practical floor |
| --- | --- | --- | --- |
| Ring creation and teardown | `io_uring_queue_init()`, `io_uring_queue_exit()` | `IoUringRing` wrapper and kernel gate | Core API, introduced with io_uring in 5.1; no minimum supported kernel has been declared. |
| File reads and writes | `IORING_OP_READ`, `IORING_OP_WRITE` via `io_uring_prep_read/write()` | File awaitables and integration tests | Core operations; short transfers and EOF are handled explicitly. |
| TCP accept, receive, and send | `IORING_OP_ACCEPT`, `IORING_OP_RECV`, `IORING_OP_SEND` | TCP listener and stream awaitables | Single-shot operations only; multishot accept/recv are not used. |
| Cancellation | `IORING_OP_ASYNC_CANCEL` via `io_uring_prep_cancel()` | Executor shutdown | Used for executor-wide graceful cancellation; per-operation public cancellation is not implemented. |
| Submission | `io_uring_submit()` | Executor submission path | Core API; no SQPOLL or deferred task-run flags are enabled. |
| Blocking completion wait | `io_uring_wait_cqe()` | Executor run loop | Core API; timer-based waits are not used. |
| Nonblocking completion drain | `io_uring_peek_cqe()` | Executor completion loop | Core API; batch peeking is not used. |
| CQE consumption | `io_uring_cqe_seen()` and `io_uring_cqe_get_data()` | Stable `OperationState` routing | Core completion protocol. |
| Eventfd wakeup | Linux `eventfd()` plus `IORING_OP_READ` on the eventfd | Cross-thread `request_stop()` wake path | Used for a stop request; the eventfd itself is a Linux primitive, not an `IORING_REGISTER_EVENTFD` registration. |

The project does not currently use multishot operations, provided buffers,
buffer rings, registered files or buffers, SQPOLL, `SINGLE_ISSUER`,
`COOP_TASKRUN`, `DEFER_TASKRUN`, minimum-timeout waits, zero-copy send/receive,
ring resizing, cBPF filtering, or `io_uring_register_query()`.

## How to interpret the matrix

- A kernel version is not sufficient to establish support. Probe required
  opcodes with `io_uring_get_probe()`/`io_uring_opcode_supported()` and inspect
  ring feature bits returned during setup.
- A successful compile only proves that the installed liburing headers expose
  an API. The running kernel may still reject an operation with `-EINVAL`,
  `-EOPNOTSUPP`, `-EPERM`, or another error.
- `EPERM` from ring creation can indicate seccomp or policy denial rather than
  an old kernel. The integration tests report that environmental condition as
  a skip.
- liburing version and kernel version are independent. A newer liburing can
  expose helpers for a feature that an older kernel cannot execute.
- The theoretical 5.1 floor above is not a declared project minimum. The
  minimum supported kernel, liburing, and compiler versions remain open Phase 7
  work in the architectural checklist.

## Primary references

- [Linux io_uring UAPI header](https://github.com/torvalds/linux/blob/master/include/uapi/linux/io_uring.h)
- [Linux io_uring zero-copy receive documentation](https://www.kernel.org/doc/html/v6.17/networking/iou-zcrx.html)
- [Linux kernel commit adding cBPF io_uring filters](https://git.kernel.org/torvalds/c/591beb0e3a03258ef9c01893a5209845799a7c33)
- [Linux Kernel Archives release status](https://www.kernel.org/)
- [io_uring(7) programming model and operation documentation](https://man7.org/linux/man-pages/man7/io_uring.7.html)
- [liburing source and release history](https://github.com/axboe/liburing)
