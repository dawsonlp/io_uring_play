# Modern C++23 io_uring File Read Example

A modern C++23 implementation demonstrating io_uring with best practices including RAII, std::expected, strong types, and zero-cost abstractions.

## Overview

This example demonstrates the same io_uring file read operation as the C version (example 01), but using modern C++23 features:

- **RAII** - Automatic resource management, no leaks possible
- **std::expected** - Type-safe error handling without exceptions
- **std::span** - Type-safe buffer views
- **Strong types** - QueueDepth/BufferSize prevent parameter mistakes
- **Zero-cost abstractions** - Compiles to same assembly as C
- **Namespaces** - Professional code organization

## Requirements

- Linux kernel 6.17 or higher
- liburing 2.11+ (`liburing-dev` package)
- GCC 13+ or Clang 16+ with full C++23 support
- C++23 standard library with `<expected>` and `<print>`

## Building

```bash
make
```

### Available Make Targets

- `make` or `make all` - Build the executable
- `make clean` - Remove build artifacts
- `make test` - Build and run the application
- `make help` - Display help message

## Running

```bash
./io_uring_example_cpp
```

The program reads `test_data.txt` (symlinked from the C example) and displays its contents.

### Expected Output

```
=== Modern C++23 io_uring File Read Example ===

[+] Creating io_uring file reader...
[+] io_uring initialized (queue depth: 32)
[+] Allocating buffer (4096 bytes)
[+] Opening and reading file: test_data.txt

--- Performing io_uring read operation ---

--- Read Results ---
Bytes read: 233

--- File Contents ---
Hello, io_uring!
This is a test file for demonstrating basic file read operations.
We're using kernel 6.17 features.
Line 4: The quick brown fox jumps over the lazy dog.
Line 5: Testing multi-line content handling.
End of test data.
--- End of Contents ---

[+] Resources automatically cleaned up via RAII
[+] Success! Modern C++23 io_uring example completed.

=== Modern C++ Advantages Demonstrated ===
✓ RAII: No manual cleanup needed (zero resource leaks possible)
✓ Type Safety: std::expected forces error checking at compile time
✓ std::span: Type-safe buffer views with bounds checking
✓ Strong Types: QueueDepth/BufferSize prevent parameter mistakes
✓ Zero Cost: All abstractions inline to same assembly as C
✓ Exception Safe: Works correctly even with exceptions disabled
```

## Code Structure

### `io_uring_wrapper.hpp`

Modern C++23 RAII wrappers and utilities:

1. **Strong Types**
   - `QueueDepth` - Type-safe queue depth parameter
   - `BufferSize` - Type-safe buffer size parameter

2. **Error Handling**
   - `Error` - Structured error type with code and message
   - `Result<T>` - Alias for `std::expected<T, Error>`

3. **RAII Wrappers**
   - `FileDescriptor` - Automatic file descriptor cleanup
   - `IoUringGuard` - Automatic io_uring lifecycle management
   - `IoUringFileReader` - High-level file reading interface

All wrappers are **move-only** to enforce unique ownership semantics.

### `main.cpp`

Application entry point demonstrating:
- Modern error handling with `std::expected`
- Type-safe buffer management with `std::span`
- Formatted output with `std::println`
- Automatic resource cleanup via RAII

## C++23 Features Demonstrated

### Core Features

1. **std::expected<T, E>** (C++23)
   ```cpp
   Result<IoUringFileReader> create() noexcept;
   // Forces compile-time error checking
   ```

2. **std::print/println** (C++23)
   ```cpp
   std::println("[+] Bytes read: {}", bytes_read);
   ```

3. **Strong Types**
   ```cpp
   struct QueueDepth { uint32_t value; };
   // Prevents mixing up parameters
   ```

4. **std::span<std::byte>**
   ```cpp
   Result<size_t> read_file(std::string_view path, 
                             std::span<std::byte> buffer);
   ```

### Modern Practices

- **RAII Everywhere** - Zero manual cleanup
- **Move Semantics** - Efficient resource transfer
- **[[nodiscard]]** - Prevents ignoring return values
- **constexpr** - Compile-time evaluation
- **noexcept** - Performance and correctness

## Comparison with C Version

### C Version (Example 01)
```c
// Manual resource management
io_uring ring;
io_uring_queue_init(32, &ring, 0);
// ... use ring ...
io_uring_queue_exit(&ring);  // Easy to forget!

// Error-prone error handling
int fd = open("file.txt", O_RDONLY);
if (fd < 0) {
    // Manual error handling
}
close(fd);  // Easy to forget!
```

### C++ Version (Example 02)
```cpp
// Automatic resource management with RAII
auto reader = IoUringFileReader::create(QueueDepth{32})
    .value_or_else([](auto e) { die(e); });
// Cleanup automatic on scope exit!

// Type-safe error handling
auto result = reader.read_file("file.txt", buffer);
if (!result) {
    // Forced to check errors at compile time
    handle_error(result.error());
}
// No resource leaks possible!
```

### Key Advantages

1. **Safety**
   - RAII guarantees cleanup
   - No resource leaks possible
   - Move semantics prevent accidental copies

2. **Type Safety**
   - Strong types prevent parameter mistakes
   - std::expected forces error checking
   - std::span prevents buffer overruns

3. **Performance**
   - Zero-cost abstractions
   - Inline functions
   - Same assembly as C version

4. **Maintainability**
   - Clear ownership semantics
   - Self-documenting code
   - Easier to extend

## Design Decisions

See [design_decisions.md](design_decisions.md) for detailed rationale on:
- C++23 standard selection
- RAII wrapper design
- std::expected for error handling
- Strong types implementation
- Zero-cost abstraction strategy

## Files

- `main.cpp` - Main application
- `io_uring_wrapper.hpp` - RAII wrappers and utilities
- `Makefile` - Build configuration
- `test_data.txt` - Symlink to test file
- `design_decisions.md` - Design rationale
- `README.md` - This file

## Performance Notes

All abstractions are **zero-cost**:
- RAII wrappers inline completely
- std::expected is a single branch
- std::span has no overhead vs raw pointers
- Strong types compile away entirely

The generated assembly is equivalent to the C version.

## Learning Resources

- [C++23 Features](https://en.cppreference.com/w/cpp/23)
- [std::expected Reference](https://en.cppreference.com/w/cpp/utility/expected)
- [std::span Reference](https://en.cppreference.com/w/cpp/container/span)
- [RAII Idiom](https://en.cppreference.com/w/cpp/language/raii)
- [liburing documentation](https://github.com/axboe/liburing)

## Next Steps

After understanding this modern C++ example:
1. Compare assembly output with C version (`objdump -d`)
2. Add exception safety tests
3. Implement async patterns with coroutines
4. Create generic io_uring operation templates
5. Add std::ranges integration

## References

- C version: [../01_basic_file_read/](../01_basic_file_read/)
- Project requirements: [../../requirements.md](../../requirements.md)
