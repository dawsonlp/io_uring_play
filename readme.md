# io_uring Learning Project

This repository explores Linux `io_uring` through progressively larger examples,
from one file read in C to a tested C++23 coroutine runtime. It is an educational
project, not a production library or a source of benchmark-backed performance
claims.

## Choose an example

| Example | Role | Language | Current status |
| --- | --- | --- | --- |
| [`01_basic_file_read`](examples/01_basic_file_read/) | Minimal SQE/CQE file read | C | Runnable introductory example |
| [`02_basic_file_read_cpp`](examples/02_basic_file_read_cpp/) | RAII wrapper around the basic read | C++ | Runnable introductory example |
| [`03_async_echo_server`](examples/03_async_echo_server/) | Early async-server design exploration | C++20 design | Design material, not an implementation |
| [`04_coroutine_io`](examples/04_coroutine_io/) | Earlier coroutine runtime | C++20 | Experimental predecessor with different guarantees |
| [`04a_coroutine_io_cpp23`](examples/04a_coroutine_io_cpp23/) | Correctness-focused coroutine runtime | C++23 | Actively developed and extensively tested |

The repository has not yet formally designated one coroutine implementation as
canonical. The C++23 example currently has the most complete lifecycle,
cancellation, testing, and behavior documentation. Do not assume fixes or
guarantees in `04a` also exist in the C++20 predecessor.

## Suggested reading paths

For a first look at `io_uring`:

1. Build [`01_basic_file_read`](examples/01_basic_file_read/).
2. Compare its manual resource management with
   [`02_basic_file_read_cpp`](examples/02_basic_file_read_cpp/).
3. Read the [`io_uring(7)` manual](https://man7.org/linux/man-pages/man7/io_uring.7.html)
   for the kernel programming model.

For coroutine runtime and ownership design:

1. Start with the [C++23 overview](examples/04a_coroutine_io_cpp23/README.md).
2. Follow its document map for public I/O semantics, lifetime rules, operation
   transitions, architecture, and roadmap status.

The `03` documents and the C++20 architecture notes are useful as historical
design exploration. They contain proposals and goals that are not necessarily
implemented or verified.

## Requirements

All runnable examples require Linux, GNU Make, and the `liburing` development
headers and library. Language requirements vary by example:

| Examples | Compiler requirement |
| --- | --- |
| `01_basic_file_read` | C11-capable C compiler |
| `02_basic_file_read_cpp` | C++23-capable C++ compiler |
| `04_coroutine_io` | C++20-capable C++ compiler with coroutine support |
| `04a_coroutine_io_cpp23` | C++23-capable C++ compiler with `std::expected` and `std::print` |

The C++23 build uses `-std=c++23`. Its most recently verified development
environment used GCC 15.2.0, `liburing` 2.15, and Linux 7.0.0; these are verified
versions, not declared minimums. See the
[C++23 requirements and test instructions](examples/04a_coroutine_io_cpp23/README.md#requirements)
for details.

The project does not currently declare repository-wide minimum kernel,
`liburing`, GCC, or Clang versions. Establishing and testing those minimums is an
open portability task for the C++23 runtime.

## Quick start

Basic C example:

```bash
make -C examples/01_basic_file_read
./examples/01_basic_file_read/io_uring_example
```

C++23 runtime and complete test matrix:

```bash
make -C examples/04a_coroutine_io_cpp23 MODE=debug test
make -C examples/04a_coroutine_io_cpp23 matrix
```

Live `io_uring` tests can be unavailable under some seccomp or container
policies. The C++23 integration runner reports those environmental cases as
skips while continuing to run its kernel-independent unit suite.

## Repository-level documents

- [`requirements.md`](requirements.md) is the original requirements brief for
  the first basic example. It is historical context, not a current support
  policy for every directory.
- [`KERNEL_FEATURES_ROADMAP.md`](KERNEL_FEATURES_ROADMAP.md) is an exploratory
  feature-planning snapshot. It is not a statement that listed features are
  implemented.
- [`IO_URING_KERNEL_FEATURE_MATRIX.md`](IO_URING_KERNEL_FEATURE_MATRIX.md)
  maps kernel feature milestones to the operations used by the project.
- The C++23
  [`ARCHITECTURAL_CODE_QUALITY_CHECKLIST.md`](examples/04a_coroutine_io_cpp23/ARCHITECTURAL_CODE_QUALITY_CHECKLIST.md)
  is the authoritative status and roadmap for that implementation.

## References

- [Linux `io_uring(7)` manual](https://man7.org/linux/man-pages/man7/io_uring.7.html)
- [liburing repository and manual pages](https://github.com/axboe/liburing)
- [LWN io_uring coverage](https://lwn.net/Kernel/Index/#io_uring)

## Licensing

No repository license file is currently present. The educational purpose does
not itself grant reuse or redistribution rights; an explicit license should be
added before treating the code as reusable by third parties.
