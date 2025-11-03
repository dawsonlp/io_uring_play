/**
 * @file test_task.cpp
 * @brief Test Task<T> coroutine implementation
 */

#include "include/io_uring_async/core/task.hpp"
#include "include/io_uring_async/core/result.hpp"
#include "include/io_uring_async/core/error.hpp"
#include <iostream>

using namespace io_uring_async;

// Simple coroutine that returns a value
Task<int> return_value() {
    co_return 42;
}

// Coroutine that awaits another coroutine
Task<int> chain_values() {
    int x = co_await return_value();
    co_return x * 2;
}

// Void coroutine
Task<void> void_coroutine() {
    co_return;
}

// Coroutine that awaits void coroutine
Task<int> use_void() {
    co_await void_coroutine();
    co_return 100;
}

// Manual execution of a coroutine (without executor)
template<typename T>
T run_sync(Task<T>&& task) {
    // For testing: manually drive the coroutine
    auto handle = std::coroutine_handle<typename Task<T>::promise_type>::from_address(
        reinterpret_cast<void*>(&task)
    );
    
    // Since tasks are lazy, we need to start them
    // In real usage, the executor would do this
    // For now, just validate compilation
    if constexpr (std::is_void_v<T>) {
        return;
    } else {
        return T{};  // Placeholder for test
    }
}

int main() {
    std::cout << "Testing Task<T> compilation...\n";
    
    // Test that we can create tasks
    auto task1 = return_value();
    std::cout << "Created task1\n";
    
    auto task2 = void_coroutine();
    std::cout << "Created task2\n";
    
    // Test move semantics
    auto task3 = std::move(task1);
    std::cout << "Moved task1 to task3\n";
    
    // Test that tasks are move-only (this shouldn't compile):
    // auto task4 = task3;  // Error: deleted copy constructor
    
    std::cout << "Task<T> compilation test passed!\n";
    std::cout << "Note: Full execution requires the executor (Phase 4)\n";
    
    return 0;
}
