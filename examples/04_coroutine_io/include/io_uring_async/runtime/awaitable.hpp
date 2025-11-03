/**
 * @file awaitable.hpp
 * @brief Awaitable wrappers for io_uring operations
 * 
 * Provides awaitable types that integrate I/O operations with the executor.
 * These types implement the coroutine awaitable interface and manage the
 * lifecycle of async operations.
 */

#pragma once

#include "../core/result.hpp"
#include "../core/error.hpp"
#include "../core/io_uring_wrapper.hpp"
#include <coroutine>
#include <cstddef>
#include <span>

namespace io_uring_async {

// Forward declaration
class IoUringExecutor;

/**
 * @brief Base awaitable for io_uring operations
 * 
 * This template provides the common awaitable interface for I/O operations.
 * Derived classes specify the operation type and result extraction.
 * 
 * @tparam T The result type of the operation
 */
template<typename T>
class IoUringAwaitable {
public:
    /**
     * @brief Construct an awaitable
     * 
     * @param executor Reference to the executor managing this operation
     */
    explicit IoUringAwaitable(IoUringExecutor& executor) noexcept
        : executor_(&executor)
        , operation_id_(0)
        , result_(0) {}
    
    /**
     * @brief Check if the operation is ready (always false for async ops)
     */
    bool await_ready() const noexcept {
        return false;  // Always suspend for I/O operations
    }
    
    /**
     * @brief Called when coroutine suspends
     * 
     * Submits the operation to io_uring and registers the continuation.
     * 
     * @param awaiter The coroutine handle of the waiting coroutine
     */
    void await_suspend(std::coroutine_handle<> awaiter);
    
    /**
     * @brief Called when coroutine resumes
     * 
     * Extracts and returns the operation result.
     * 
     * @return Result<T, Error> containing the operation outcome
     */
    Result<T, Error> await_resume();
    
protected:
    IoUringExecutor* executor_;  ///< Executor managing this operation
    uint64_t operation_id_;      ///< Unique ID for this operation
    int result_;                 ///< Raw io_uring result
    
    /**
     * @brief Submit the actual I/O operation to io_uring
     * 
     * Must be implemented by derived classes to prepare the specific
     * operation type (read, write, accept, etc.)
     * 
     * @return true if operation was submitted, false if queue full
     */
    virtual bool submit_operation() = 0;
    
    /**
     * @brief Convert raw io_uring result to typed result
     * 
     * Must be implemented by derived classes to interpret the result
     * based on the operation type.
     * 
     * @param res The raw io_uring result (bytes or negative errno)
     * @return Result<T, Error> with typed result or error
     */
    virtual Result<T, Error> process_result(int res) = 0;
};

/**
 * @brief Awaitable for read operations
 */
class ReadAwaitable : public IoUringAwaitable<size_t> {
public:
    ReadAwaitable(IoUringExecutor& executor, int fd, std::span<std::byte> buffer, uint64_t offset)
        : IoUringAwaitable(executor)
        , fd_(fd)
        , buffer_(buffer)
        , offset_(offset) {}
    
protected:
    bool submit_operation() override;
    Result<size_t, Error> process_result(int res) override;
    
private:
    int fd_;
    std::span<std::byte> buffer_;
    uint64_t offset_;
};

/**
 * @brief Awaitable for write operations
 */
class WriteAwaitable : public IoUringAwaitable<size_t> {
public:
    WriteAwaitable(IoUringExecutor& executor, int fd, std::span<const std::byte> buffer, uint64_t offset)
        : IoUringAwaitable(executor)
        , fd_(fd)
        , buffer_(buffer)
        , offset_(offset) {}
    
protected:
    bool submit_operation() override;
    Result<size_t, Error> process_result(int res) override;
    
private:
    int fd_;
    std::span<const std::byte> buffer_;
    uint64_t offset_;
};

/**
 * @brief Awaitable for accept operations
 */
class AcceptAwaitable : public IoUringAwaitable<int> {
public:
    AcceptAwaitable(IoUringExecutor& executor, int fd)
        : IoUringAwaitable(executor)
        , fd_(fd)
        , addr_{}
        , addrlen_(sizeof(addr_)) {}
    
    /**
     * @brief Get the client address after successful accept
     */
    const sockaddr_storage& client_addr() const noexcept { return addr_; }
    socklen_t addr_len() const noexcept { return addrlen_; }
    
protected:
    bool submit_operation() override;
    Result<int, Error> process_result(int res) override;
    
private:
    int fd_;
    sockaddr_storage addr_;
    socklen_t addrlen_;
};

/**
 * @brief Awaitable for send operations
 */
class SendAwaitable : public IoUringAwaitable<size_t> {
public:
    SendAwaitable(IoUringExecutor& executor, int fd, std::span<const std::byte> buffer, int flags = 0)
        : IoUringAwaitable(executor)
        , fd_(fd)
        , buffer_(buffer)
        , flags_(flags) {}
    
protected:
    bool submit_operation() override;
    Result<size_t, Error> process_result(int res) override;
    
private:
    int fd_;
    std::span<const std::byte> buffer_;
    int flags_;
};

/**
 * @brief Awaitable for receive operations
 */
class RecvAwaitable : public IoUringAwaitable<size_t> {
public:
    RecvAwaitable(IoUringExecutor& executor, int fd, std::span<std::byte> buffer, int flags = 0)
        : IoUringAwaitable(executor)
        , fd_(fd)
        , buffer_(buffer)
        , flags_(flags) {}
    
protected:
    bool submit_operation() override;
    Result<size_t, Error> process_result(int res) override;
    
private:
    int fd_;
    std::span<std::byte> buffer_;
    int flags_;
};

/**
 * @brief Awaitable for close operations
 */
class CloseAwaitable : public IoUringAwaitable<void> {
public:
    CloseAwaitable(IoUringExecutor& executor, int fd)
        : IoUringAwaitable(executor)
        , fd_(fd) {}
    
protected:
    bool submit_operation() override;
    Result<void, Error> process_result(int res) override;
    
private:
    int fd_;
};

} // namespace io_uring_async
