# io_uring_async - C++20 Coroutine-Based io_uring Library

A modern, educational async I/O library for Linux that combines C++20 coroutines with io_uring.

## Features

- **Native C++20 Coroutines**: Clean async/await syntax
- **io_uring Integration**: High-performance Linux async I/O
- **Type-Safe**: Result<T,E> error handling, strong typing
- **RAII**: Automatic resource management
- **Zero-Copy**: Support for registered buffers
- **Batching**: Automatic operation batching for performance

## Requirements

- Linux kernel 6.17+ (5.19+ minimum)
- GCC 11+ or Clang 14+ with C++20 support
- liburing library

## Building

```bash
# Release build
make

# Debug build
make debug

# With sanitizers
make asan  # Address sanitizer
make ubsan # Undefined behavior sanitizer

# Run tests
make test

# Clean
make clean
```

## Quick Example

```cpp
#include <io_uring_async/io_uring_async.hpp>

using namespace io_uring_async;

Task<void> handle_client(TcpStream client) {
    std::array<std::byte, 4096> buffer;
    
    while (true) {
        auto result = co_await client.receive(buffer);
        if (!result || result.value() == 0) break;
        
        co_await client.send(std::span{buffer.data(), result.value()});
    }
}

int main() {
    IoUringExecutor executor;
    
    executor.spawn([]() -> Task<void> {
        auto listener = co_await TcpListener::bind(8080);
        
        while (true) {
            auto client = co_await listener.accept();
            executor.spawn(handle_client(std::move(client)));
        }
    }());
    
    executor.run();
}
```

## Documentation

- [DESIGN.md](DESIGN.md) - High-level architecture
- [ARCHITECTURE_NOTES.md](ARCHITECTURE_NOTES.md) - Implementation guidance
- [design_decisions.md](design_decisions.md) - Design rationale
- [DEVELOPMENT_CHECKLIST.md](DEVELOPMENT_CHECKLIST.md) - Implementation progress

## Status

🚧 **Under Development** - Following the development checklist

## License

See LICENSE file for details.
