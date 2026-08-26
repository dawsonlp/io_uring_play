# io_uring Learning Project

Learn and explore Linux io_uring through progressive examples, from basic C file I/O to advanced C++20 coroutine-based async networking.

## Project Overview

This project demonstrates modern async I/O on Linux using io_uring, progressing from simple to advanced usage patterns:

| Example | Description | Language | Features |
|---------|-------------|----------|----------|
| `01_basic_file_read` | Basic io_uring file read | C | SQE/CQE basics |
| `02_basic_file_read_cpp` | RAII wrapper for io_uring | C++ | Type safety, RAII |
| `03_async_echo_server` | Design docs for async server | Design | Architecture |
| `04_coroutine_io` | Full coroutine-based async I/O library | C++20 | Coroutines, Task<T> |
| `04a_coroutine_io_cpp23` | C++23 variant with std::expected | C++23 | std::expected |

## Requirements

- **Linux Kernel**: 6.17+ (5.19+ minimum with degraded features)
- **Compiler**: GCC 11+ or Clang 14+ for C++20 coroutines
- **Library**: liburing (latest version)

## Quick Start

```bash
# Build the basic C example
cd examples/01_basic_file_read
make
./io_uring_example

# Build the C++20 coroutine library example
cd examples/04_coroutine_io
make
./test_library
```

## Documentation

### Core Documentation

- **[requirements.md](requirements.md)** - Project requirements and goals
- **[KERNEL_FEATURES_ROADMAP.md](KERNEL_FEATURES_ROADMAP.md)** - io_uring kernel feature adoption roadmap

### Example-Specific Documentation

Each example directory contains:
- `README.md` - Example-specific overview
- `design_decisions.md` - Design decisions log
- Additional architecture/design documents as needed

### Key Design Documents (04_coroutine_io)

- [DESIGN.md](examples/04_coroutine_io/DESIGN.md) - Comprehensive design document
- [ARCHITECTURE_NOTES.md](examples/04_coroutine_io/ARCHITECTURE_NOTES.md) - Technical implementation notes
- [USER_GUIDE.md](examples/04_coroutine_io/USER_GUIDE.md) - How to use the library

## Roadmap

See **[KERNEL_FEATURES_ROADMAP.md](KERNEL_FEATURES_ROADMAP.md)** for the planned adoption of modern io_uring features:

### Phase 1: Setup Optimizations (Planned)
- IORING_SETUP_SINGLE_ISSUER
- IORING_SETUP_DEFER_TASKRUN
- IORING_SETUP_COOP_TASKRUN
- Timeout-based wait operations

### Phase 2: Network Optimizations (Planned)
- Multishot accept
- Cancel operations
- SQE linking

### Phase 3: High-Performance Features (Planned)
- Provided buffers / buffer rings
- Multishot recv
- Registered buffers

## References

- [Linux `io_uring(7)` manual](https://man7.org/linux/man-pages/man7/io_uring.7.html)
- [liburing repository](https://github.com/axboe/liburing)
- [LWN io_uring coverage](https://lwn.net/Kernel/Index/#io_uring)

## License

Educational project for learning io_uring.
