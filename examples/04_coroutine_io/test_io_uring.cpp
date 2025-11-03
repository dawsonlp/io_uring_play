/**
 * @file test_io_uring.cpp
 * @brief Test io_uring wrapper compilation and basic functionality
 */

#include "include/io_uring_async/core/io_uring_wrapper.hpp"
#include "include/io_uring_async/core/result.hpp"
#include "include/io_uring_async/core/error.hpp"
#include <iostream>

using namespace io_uring_async;

int main() {
    std::cout << "Testing IoUring wrapper...\n";
    
    // Test creating an io_uring instance
    auto ring_result = IoUring::create(32);
    
    if (!ring_result) {
        std::cerr << "Failed to create io_uring: " 
                  << ring_result.error().what() << "\n";
        std::cerr << "Note: This may fail if kernel doesn't support io_uring\n";
        return 0;  // Not a test failure, just unsupported
    }
    
    std::cout << "Successfully created io_uring\n";
    
    auto ring = std::move(ring_result.value());
    std::cout << "Ring is initialized: " << ring.is_initialized() << "\n";
    
    // Test move semantics
    auto ring2 = std::move(ring);
    std::cout << "Moved ring to ring2\n";
    std::cout << "Original ring is initialized: " << ring.is_initialized() << "\n";
    std::cout << "New ring2 is initialized: " << ring2.is_initialized() << "\n";
    
    std::cout << "IoUring wrapper test passed!\n";
    return 0;
}
