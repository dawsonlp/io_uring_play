# Basic io_uring File Read Example

A simple, educational example demonstrating io_uring fundamentals on Linux kernel 6.17+.

## Overview

This example demonstrates:
- Initializing an io_uring instance
- Preparing and submitting a file read operation
- Waiting for and processing completion events
- Proper cleanup and error handling

## Requirements

- Linux kernel 6.17 or higher
- liburing 2.11+ (`liburing-dev` package)
- GCC or Clang with C11 support

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
./io_uring_example
```

The program will read `test_data.txt` using io_uring and display its contents.

### Expected Output

```
=== io_uring Basic File Read Example ===

[+] io_uring initialized successfully (queue depth: 32)
[+] Opening file: test_data.txt
[+] File opened successfully (fd: 4)

--- Performing io_uring read operation ---
[+] Got submission queue entry
[+] Prepared read operation (buffer size: 4095 bytes)
[+] Submitted operation to kernel (1 operation(s))
[+] Operation completed

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

[+] Cleaning up resources
[+] Success! io_uring example completed.
```

## Code Structure

### `main.c`

The main source file contains:

1. **setup_io_uring()** - Initializes the io_uring instance with a queue depth of 32
2. **read_file_with_uring()** - Demonstrates the complete io_uring workflow:
   - Get a submission queue entry (SQE)
   - Prepare the read operation
   - Submit to kernel
   - Wait for completion
   - Process completion queue entry (CQE)
3. **main()** - Orchestrates the example flow

### Key io_uring Concepts Demonstrated

- **Queue Initialization**: `io_uring_queue_init()`
- **Submission Queue Entry (SQE)**: Request structure for operations
- **Completion Queue Entry (CQE)**: Result structure for completed operations
- **Submission**: `io_uring_submit()` sends operations to kernel
- **Waiting**: `io_uring_wait_cqe()` blocks until completion
- **Cleanup**: `io_uring_queue_exit()` tears down the ring

## Design Decisions

See [design_decisions.md](design_decisions.md) for detailed rationale behind:
- Choice of file read operation
- Queue depth of 32 entries
- Buffer size of 4KB
- Error handling strategy
- Use of liburing vs raw syscalls

## Files

- `main.c` - Main application source
- `Makefile` - Build configuration
- `test_data.txt` - Sample test file
- `design_decisions.md` - Design rationale documentation
- `CHECKLIST.md` - Development checklist
- `README.md` - This file

## Learning Resources

- [liburing documentation](https://github.com/axboe/liburing)
- [Linux `io_uring(7)` manual](https://man7.org/linux/man-pages/man7/io_uring.7.html)
- [Linux man pages](https://man7.org/linux/man-pages/man7/io_uring.7.html)

## Next Steps

After understanding this basic example, consider exploring:
1. Multiple concurrent operations
2. Different operation types (write, socket I/O)
3. Advanced features (registered buffers, multishot operations)
4. Kernel 6.17 specific features
5. Performance benchmarking

## References

See [../../requirements.md](../../requirements.md) for the full project requirements.
