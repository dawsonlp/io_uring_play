# C++26 Completion-Oriented Execution Experiment

**Status: initial sender/receiver and single-shot file-read smoke tests.**

This directory is reserved for an independent experiment with C++26 `std::execution` (initially NVIDIA `stdexec`) and Linux `io_uring`. The aim is to compare a sender/receiver completion model against direct `liburing` and the existing `../04a_coroutine_io_cpp23/` coroutine runtime—not to replace that tested baseline.

See [the project proposal](../../EXECUTION_MODEL_PROPOSAL.md) for hypotheses, architectural invariants, phases, and evaluation criteria.

## Where work will happen

The repository may be edited and coordinated from a **personal MacBook Air**. That Mac is **not** the Linux target and cannot validate Linux `io_uring` behavior.

Implementation, builds, and tests are planned for a separate **Ubuntu 26.10 / Linux 7.3 x86-64 Dell Precision 5570**. Its toolchain includes GCC 16.2, Clang 22.1.8, and liburing 2.15.

## C++23 baseline

On 2026-11-20, `make -C examples/04a_coroutine_io_cpp23 -j4 matrix` passed on Linux 7.3.0-10-generic with liburing 2.15 and the baseline Makefile's default GCC 15.3. The matrix covers release, debug, AddressSanitizer, and UndefinedBehaviorSanitizer. Ring creation returned `EPERM` in the sandbox. Run the integration tests outside that sandbox to reproduce the baseline with kernel I/O enabled.

Meson 1.10.1 and Ninja 1.13.2 built and ran both experiment tests with GCC 16.2 and Clang 22.1.8. The file-read test ran with kernel access outside the sandbox.

## Build and test

Run from this directory. The first setup fetches the exact NVIDIA `stdexec` commit in `subprojects/stdexec.wrap` and requires access to GitHub.

```sh
CXX=g++-16 meson setup build-gcc
meson test -C build-gcc --print-errorlogs
CXX=clang++-22 meson setup build-clang
meson test -C build-clang --print-errorlogs
```

`execution_smoke` uses NVIDIA's implementation of sender/receiver operations, composition, scheduling, and coroutine interop. It verifies inline completion that destroys the operation state, a continuation that explicitly moves to a worker thread, and a scheduler honoring a pre-requested stop. The pinned revision's upstream `examples/io_uring.cpp` also ran successfully with both compilers. That example exercises the experimental `exec::io_uring_context` timer scheduler.

`read_smoke` implements a local sender over `liburing`. It checks a short read with known data, EOF, and a bad descriptor. Each receiver destroys its operation state, and the first two start the next read inline after the CQE has been retired. The test skips if ring creation is denied or unavailable. The prototype uses a borrowed buffer and descriptor, permits one request at a time, and aborts on ring submission or wait failure.

The next phase covers kernel cancellation, ownership and shutdown under failure. The dependent and bounded concurrent read comparisons follow that phase.
