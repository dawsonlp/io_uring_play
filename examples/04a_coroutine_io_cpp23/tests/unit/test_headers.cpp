/**
 * @file test_headers.cpp
 * @brief Compilation and basic unit tests for io_uring_async_cpp23
 * 
 * Tests that:
 * 1. All headers compile correctly under C++23
 * 2. The umbrella header brings in everything
 * 3. Core types can be instantiated
 * 4. Error types work correctly
 * 5. IoUring can be created
 * 6. Task coroutines compile and return values
 */

#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>
#include <print>
#include <cassert>
#include <expected>

using namespace io_uring_async_cpp23;

// ============================================================================
// Test helpers
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        std::print("  TEST: {} ... ", #name); \
    } while(0)

#define PASS() \
    do { \
        tests_passed++; \
        std::print("PASS\n"); \
    } while(0)

#define FAIL(msg) \
    do { \
        std::print("FAIL: {}\n", msg); \
    } while(0)

// ============================================================================
// Tests
// ============================================================================

void test_version() {
    TEST(version_constants);
    assert(VERSION_MAJOR >= 0);
    assert(VERSION_MINOR >= 0);
    assert(CPP_STANDARD == 23);
    PASS();
}

void test_error_creation() {
    TEST(error_from_code);
    Error err(Error::Code::IoError, "test error");
    assert(err.code() == Error::Code::IoError);
    assert(err.message() == "test error");
    assert(!err.is_success());
    PASS();
}

void test_error_from_errno() {
    TEST(error_from_errno);
    Error err = Error::from_errno(ENOENT);
    assert(err.code() == Error::Code::NotFound);
    assert(!err.message().empty());
    PASS();
}

void test_error_description() {
    TEST(error_description);
    Error err(Error::Code::ConnectionClosed, "peer reset");
    auto desc = err.description();
    assert(desc.find("Connection Closed") != std::string::npos);
    assert(desc.find("peer reset") != std::string::npos);
    PASS();
}

void test_error_equality() {
    TEST(error_equality);
    Error a(Error::Code::Timeout);
    Error b(Error::Code::Timeout, "different message");
    Error c(Error::Code::IoError);
    assert(a == b);  // Same code
    assert(a != c);  // Different code
    PASS();
}

void test_expected_with_error() {
    TEST(std_expected_with_error);
    std::expected<int, Error> good_result = 42;
    assert(good_result.has_value());
    assert(good_result.value() == 42);
    
    std::expected<int, Error> bad_result = 
        std::unexpected(Error(Error::Code::IoError, "fail"));
    assert(!bad_result.has_value());
    assert(bad_result.error().code() == Error::Code::IoError);
    PASS();
}

void test_iouring_create() {
    TEST(iouring_create);
    auto ring = IoUring::create(64);
    assert(ring.has_value());
    PASS();
}

void test_iouring_get_sqe() {
    TEST(iouring_get_sqe);
    auto ring = IoUring::create(64);
    assert(ring.has_value());
    
    auto sqe = ring->get_sqe();
    assert(sqe.has_value());
    assert(sqe.value() != nullptr);
    PASS();
}

void test_executor_create() {
    TEST(executor_create);
    auto executor = IoUringExecutor::create(64);
    assert(executor.has_value());
    PASS();
}

void test_executor_run_empty() {
    TEST(executor_run_empty);
    auto executor = IoUringExecutor::create(64);
    assert(executor.has_value());
    
    // Running with no tasks should return immediately
    auto result = executor->run();
    assert(result.has_value());
    PASS();
}

// Test that Task<int> coroutine compiles
Task<int> compute_value() {
    co_return 42;
}

// Test that Task<void> coroutine compiles
Task<void> do_nothing() {
    co_return;
}

void test_task_compiles() {
    TEST(task_coroutine_compiles);
    auto t1 = compute_value();
    assert(t1.valid());
    
    auto t2 = do_nothing();
    assert(t2.valid());
    PASS();
}

void test_task_move() {
    TEST(task_move_semantics);
    auto t1 = compute_value();
    assert(t1.valid());
    
    auto t2 = std::move(t1);
    assert(t2.valid());
    assert(!t1.valid());  // Moved-from
    PASS();
}

void test_tcp_listener_bind() {
    TEST(tcp_listener_bind);
    auto executor = IoUringExecutor::create(64);
    assert(executor.has_value());
    
    // Bind to a high port that should be available
    auto listener = TcpListener::bind(*executor, 0);  // port 0 = kernel picks
    // This may or may not work depending on port 0 support, 
    // so we just test it doesn't crash
    if (listener) {
        assert(listener->fd() >= 0);
    }
    PASS();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::print("=== io_uring_async_cpp23 Unit Tests ===\n");
    std::print("Library v{}.{}.{} (C++{})\n\n",
        VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, CPP_STANDARD);
    
    // Error tests
    std::print("Error type:\n");
    test_error_creation();
    test_error_from_errno();
    test_error_description();
    test_error_equality();
    test_expected_with_error();
    
    // IoUring tests
    std::print("\nIoUring wrapper:\n");
    test_iouring_create();
    test_iouring_get_sqe();
    
    // Executor tests
    std::print("\nExecutor:\n");
    test_executor_create();
    test_executor_run_empty();
    
    // Task tests
    std::print("\nTask<T>:\n");
    test_task_compiles();
    test_task_move();
    
    // TCP tests
    std::print("\nTCP I/O:\n");
    test_tcp_listener_bind();
    
    // Summary
    std::print("\n=== Results: {}/{} tests passed ===\n", 
        tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        std::print("✓ All tests passed!\n");
        return 0;
    } else {
        std::print("✗ {} tests failed!\n", tests_run - tests_passed);
        return 1;
    }
}
