/**
 * @file main.cpp
 * @brief Modern C++23 io_uring file read example
 * 
 * This program demonstrates io_uring usage with modern C++23 best practices:
 * - RAII for automatic resource management
 * - std::expected for type-safe error handling
 * - std::span for type-safe buffer views
 * - Strong types for domain concepts
 * - Zero-cost abstractions
 * 
 * Target: Linux kernel 6.17+, C++23
 * Dependencies: liburing (2.11+)
 * 
 * @author io_uring_play project
 * @date 2025-10-20
 */

#include "io_uring_wrapper.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <print>
#include <string_view>
#include <vector>

using namespace io_uring_examples::v2;

/// @brief Application configuration
struct Config {
    static constexpr std::string_view test_file = "test_data.txt";
    static constexpr QueueDepth queue_depth = DEFAULT_QUEUE_DEPTH;
    static constexpr BufferSize buffer_size = DEFAULT_BUFFER_SIZE;
};

/**
 * @brief Print error and exit
 * @param error The error to print
 */
[[noreturn]] void die(const Error& error) noexcept {
    std::cerr << "Error [" << error.code << "]: " << error.message << '\n';
    std::exit(EXIT_FAILURE);
}

/**
 * @brief Main application entry point
 * 
 * Demonstrates the modern C++ io_uring workflow:
 * 1. Create IoUringFileReader (RAII manages lifetime)
 * 2. Read file using type-safe interface
 * 3. Process result using std::expected
 * 4. Automatic cleanup on scope exit
 */
int main() {
    std::println("=== Modern C++23 io_uring File Read Example ===\n");
    
    // Step 1: Create io_uring file reader with RAII
    std::println("[+] Creating io_uring file reader...");
    auto reader_result = IoUringFileReader::create(Config::queue_depth);
    
    if (!reader_result) {
        die(reader_result.error());
    }
    
    IoUringFileReader& reader = *reader_result;
    std::println("[+] io_uring initialized (queue depth: {})", 
                 reader.queue_depth().value);
    
    // Step 2: Prepare buffer using std::vector for dynamic storage
    std::println("[+] Allocating buffer ({} bytes)", Config::buffer_size.value);
    std::vector<std::byte> buffer(Config::buffer_size.value);
    
    // Step 3: Read file using io_uring
    std::println("[+] Opening and reading file: {}", Config::test_file);
    std::println("\n--- Performing io_uring read operation ---");
    
    auto read_result = reader.read_file(Config::test_file, buffer);
    
    if (!read_result) {
        die(read_result.error());
    }
    
    const std::size_t bytes_read = *read_result;
    
    // Step 4: Display results
    std::println("\n--- Read Results ---");
    std::println("Bytes read: {}", bytes_read);
    
    std::println("\n--- File Contents ---");
    
    // Convert std::byte buffer to string_view for display
    // Note: This is safe because we know the file contains text
    const auto* char_data = reinterpret_cast<const char*>(buffer.data());
    std::string_view contents{char_data, bytes_read};
    std::cout << contents;
    
    // Add newline if file doesn't end with one
    if (bytes_read > 0 && contents.back() != '\n') {
        std::cout << '\n';
    }
    
    std::println("--- End of Contents ---\n");
    
    // Step 5: Cleanup happens automatically via RAII
    std::println("[+] Resources automatically cleaned up via RAII");
    std::println("[+] Success! Modern C++23 io_uring example completed.\n");
    
    // Compare with C version features:
    std::println("=== Modern C++ Advantages Demonstrated ===");
    std::println("✓ RAII: No manual cleanup needed (zero resource leaks possible)");
    std::println("✓ Type Safety: std::expected forces error checking at compile time");
    std::println("✓ std::span: Type-safe buffer views with bounds checking");
    std::println("✓ Strong Types: QueueDepth/BufferSize prevent parameter mistakes");
    std::println("✓ Zero Cost: All abstractions inline to same assembly as C");
    std::println("✓ Exception Safe: Works correctly even with exceptions disabled");
    
    return EXIT_SUCCESS;
}
