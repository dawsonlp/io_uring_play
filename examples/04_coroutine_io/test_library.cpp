/**
 * @file test_library.cpp
 * @brief Test full library compilation
 */

#include "include/io_uring_async/io_uring_async.hpp"
#include <iostream>

using namespace io_uring_async;

// Simple test coroutine
Task<void> hello_coroutine() {
    std::cout << "Hello from coroutine!\n";
    co_return;
}

int main() {
    std::cout << "io_uring_async library v"
              << VERSION_MAJOR << "."
              << VERSION_MINOR << "."
              << VERSION_PATCH << "\n";
    
    // Test creating an executor
    auto executor_result = IoUringExecutor::create();
    
    if (!executor_result) {
        std::cerr << "Failed to create executor: "
                  << executor_result.error().what() << "\n";
        std::cerr << "Note: This may fail if kernel doesn't support io_uring\n";
        return 0;
    }
    
    std::cout << "Successfully created executor\n";
    
    // Test that we can create tasks
    auto task = hello_coroutine();
    std::cout << "Created task\n";
    
    // Note: We can't actually run the task without more infrastructure
    // (the executor needs the task to properly start), but we can verify
    // that everything compiles correctly
    
    std::cout << "Library compilation test passed!\n";
    std::cout << "Note: Full execution requires I/O awaitables (Phase 5)\n";
    
    return 0;
}
