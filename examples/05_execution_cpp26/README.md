# C++26 Completion-Oriented Execution Experiment

**Status: proposal only. No implementation or tests yet.**

This directory is reserved for an independent experiment with C++26 `std::execution` (initially NVIDIA `stdexec`) and Linux `io_uring`. The aim is to compare a sender/receiver completion model against direct `liburing` and the existing `../04a_coroutine_io_cpp23/` coroutine runtime—not to replace that tested baseline.

See [the project proposal](../../EXECUTION_MODEL_PROPOSAL.md) for hypotheses, architectural invariants, phases, and evaluation criteria.

## Where work will happen

The repository may be edited and coordinated from a **personal MacBook Air**. That Mac is **not** the Linux target and cannot validate Linux `io_uring` behavior.

Implementation, builds, and tests are planned for a separate **Ubuntu 26.10 / Linux 7.3 x86-64 Dell Precision 5570** using a **Codex agent on that machine**. Its reported toolchain includes GCC 16.2, Clang 22.1.8, and liburing 2.15. Meson and Ninja are intended for this experiment, but their installation and configuration have not yet been verified.

## First tasks for the Linux Codex agent

1. Verify host versions and unprivileged `io_uring` availability.
2. Run and record the existing C++23 debug/release/sanitizer test baseline.
3. Install/verify Meson and Ninja; pin an appropriate `stdexec` revision.
4. Build a tiny sender/receiver test before introducing kernel I/O.
5. Demonstrate one real file-read completion and document its lifetime semantics.

Do not claim any task passed until executed and evidenced on the Linux target. Keep this experiment independent of the existing examples.
