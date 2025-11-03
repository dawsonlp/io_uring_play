/**
 * @file test_build.cpp
 * @brief Minimal test to verify headers compile without warnings
 */

#include "include/io_uring_async/core/result.hpp"
#include "include/io_uring_async/core/error.hpp"
#include <iostream>

using namespace io_uring_async;

// Test function that returns Result
Result<int, Error> divide(int a, int b) {
    if (b == 0) {
        return Unexpected(Error::custom(1, "Division by zero"));
    }
    return a / b;
}

int main() {
    // Test Result<T, E>
    auto result = divide(10, 2);
    if (result) {
        std::cout << "Result: " << result.value() << "\n";
    }
    
    // Test error case
    auto error_result = divide(10, 0);
    if (!error_result) {
        std::cout << "Error: " << error_result.error().what() << "\n";
    }
    
    // Test Result<void, E>
    Result<void, Error> void_result;
    if (void_result) {
        std::cout << "Void result succeeded\n";
    }
    
    std::cout << "Build test passed!\n";
    return 0;
}
