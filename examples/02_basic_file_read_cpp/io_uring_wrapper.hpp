/**
 * @file io_uring_wrapper.hpp
 * @brief Modern C++23 RAII wrappers for io_uring
 * 
 * This header provides type-safe, exception-safe wrappers around liburing,
 * demonstrating modern C++ best practices including RAII, std::expected,
 * concepts, and zero-cost abstractions.
 * 
 * Target: C++23
 * Dependencies: liburing
 */

#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <liburing.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <fcntl.h>
#include <unistd.h>

namespace io_uring_examples::v2 {

// ============================================================================
// Strong Types for Domain Concepts
// ============================================================================

/// @brief Strong type for queue depth to prevent parameter mistakes
struct QueueDepth {
    uint32_t value;
    
    constexpr explicit QueueDepth(uint32_t v) noexcept : value(v) {}
};

/// @brief Strong type for buffer size
struct BufferSize {
    std::size_t value;
    
    constexpr explicit BufferSize(std::size_t v) noexcept : value(v) {}
};

// ============================================================================
// Constants
// ============================================================================

inline constexpr QueueDepth DEFAULT_QUEUE_DEPTH{32};
inline constexpr BufferSize DEFAULT_BUFFER_SIZE{4096};

// ============================================================================
// Error Handling
// ============================================================================

/// @brief Error type for operations that can fail
struct Error {
    int code;                    ///< Error code (typically errno)
    std::string message;         ///< Human-readable error message
    
    Error(int c, std::string msg) noexcept 
        : code(c), message(std::move(msg)) {}
    
    /// @brief Create error from errno and custom message
    static Error from_errno(std::string_view operation) noexcept {
        return Error{errno, std::string{operation} + ": " + std::strerror(errno)};
    }
    
    /// @brief Create error from liburing error code
    static Error from_uring_result(int result, std::string_view operation) noexcept {
        return Error{-result, std::string{operation} + ": " + std::strerror(-result)};
    }
};

/// @brief Result type for fallible operations
template<typename T>
using Result = std::expected<T, Error>;

// ============================================================================
// RAII File Descriptor Wrapper
// ============================================================================

/// @brief RAII wrapper for file descriptors
/// 
/// Automatically closes the file descriptor on destruction.
/// Move-only to enforce unique ownership.
class FileDescriptor {
public:
    /// @brief Construct invalid file descriptor
    constexpr FileDescriptor() noexcept : fd_(-1) {}
    
    /// @brief Take ownership of file descriptor
    constexpr explicit FileDescriptor(int fd) noexcept : fd_(fd) {}
    
    /// @brief Destructor closes the file descriptor
    ~FileDescriptor() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    
    // Move-only semantics
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    
    constexpr FileDescriptor(FileDescriptor&& other) noexcept 
        : fd_(std::exchange(other.fd_, -1)) {}
    
    constexpr FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    
    /// @brief Get the raw file descriptor
    [[nodiscard]] constexpr int get() const noexcept { return fd_; }
    
    /// @brief Check if file descriptor is valid
    [[nodiscard]] constexpr bool is_valid() const noexcept { return fd_ >= 0; }
    
    /// @brief Release ownership without closing
    [[nodiscard]] constexpr int release() noexcept {
        return std::exchange(fd_, -1);
    }
    
    /// @brief Open a file and return FileDescriptor
    [[nodiscard]] static Result<FileDescriptor> open(
        std::string_view path, 
        int flags, 
        mode_t mode = 0) noexcept 
    {
        const int fd = ::open(std::string{path}.c_str(), flags, mode);
        if (fd < 0) {
            return std::unexpected(Error::from_errno("Failed to open file"));
        }
        return FileDescriptor{fd};
    }
    
private:
    int fd_;
};

// ============================================================================
// RAII io_uring Wrapper
// ============================================================================

/// @brief RAII wrapper for io_uring instance
/// 
/// Automatically initializes on construction and tears down on destruction.
/// Move-only to enforce unique ownership.
class IoUringGuard {
public:
    /// @brief Initialize io_uring with specified queue depth
    [[nodiscard]] static Result<IoUringGuard> create(QueueDepth depth) noexcept {
        io_uring ring{};
        const int ret = io_uring_queue_init(depth.value, &ring, 0);
        if (ret < 0) {
            return std::unexpected(
                Error::from_uring_result(ret, "io_uring_queue_init failed")
            );
        }
        return IoUringGuard{ring};
    }
    
    /// @brief Destructor tears down io_uring
    ~IoUringGuard() noexcept {
        if (initialized_) {
            io_uring_queue_exit(&ring_);
        }
    }
    
    // Move-only semantics
    IoUringGuard(const IoUringGuard&) = delete;
    IoUringGuard& operator=(const IoUringGuard&) = delete;
    
    IoUringGuard(IoUringGuard&& other) noexcept 
        : ring_(other.ring_), initialized_(std::exchange(other.initialized_, false)) {}
    
    IoUringGuard& operator=(IoUringGuard&& other) noexcept {
        if (this != &other) {
            if (initialized_) {
                io_uring_queue_exit(&ring_);
            }
            ring_ = other.ring_;
            initialized_ = std::exchange(other.initialized_, false);
        }
        return *this;
    }
    
    /// @brief Get pointer to underlying io_uring structure
    [[nodiscard]] io_uring* get() noexcept { return &ring_; }
    [[nodiscard]] const io_uring* get() const noexcept { return &ring_; }
    
private:
    explicit IoUringGuard(io_uring ring) noexcept 
        : ring_(ring), initialized_(true) {}
    
    io_uring ring_;
    bool initialized_;
};

// ============================================================================
// io_uring File Reader
// ============================================================================

/// @brief High-level io_uring file reader
/// 
/// Encapsulates the complete workflow of reading a file using io_uring.
/// Demonstrates RAII, type safety, and modern C++ practices.
class IoUringFileReader {
public:
    /// @brief Construct reader with specified queue depth
    [[nodiscard]] static Result<IoUringFileReader> create(
        QueueDepth depth = DEFAULT_QUEUE_DEPTH) noexcept 
    {
        auto ring_result = IoUringGuard::create(depth);
        if (!ring_result) {
            return std::unexpected(std::move(ring_result.error()));
        }
        return IoUringFileReader{std::move(*ring_result), depth};
    }
    
    /// @brief Read file contents into buffer
    /// 
    /// @param path Path to file to read
    /// @param buffer Buffer to read into (as std::span for safety)
    /// @return Number of bytes read on success, Error on failure
    [[nodiscard]] Result<std::size_t> read_file(
        std::string_view path,
        std::span<std::byte> buffer) noexcept 
    {
        // Step 1: Open the file
        auto fd_result = FileDescriptor::open(path, O_RDONLY);
        if (!fd_result) {
            return std::unexpected(std::move(fd_result.error()));
        }
        FileDescriptor& fd = *fd_result;
        
        // Step 2: Get submission queue entry
        io_uring_sqe* sqe = io_uring_get_sqe(ring_.get());
        if (!sqe) {
            return std::unexpected(Error{-ENOMEM, "Could not get SQE"});
        }
        
        // Step 3: Prepare read operation
        io_uring_prep_read(sqe, fd.get(), buffer.data(), buffer.size(), 0);
        
        // Step 4: Submit operation
        const int submitted = io_uring_submit(ring_.get());
        if (submitted < 0) {
            return std::unexpected(
                Error::from_uring_result(submitted, "io_uring_submit failed")
            );
        }
        
        // Step 5: Wait for completion
        io_uring_cqe* cqe = nullptr;
        const int wait_ret = io_uring_wait_cqe(ring_.get(), &cqe);
        if (wait_ret < 0) {
            return std::unexpected(
                Error::from_uring_result(wait_ret, "io_uring_wait_cqe failed")
            );
        }
        
        // Step 6: Check result
        const int result = cqe->res;
        io_uring_cqe_seen(ring_.get(), cqe);
        
        if (result < 0) {
            return std::unexpected(
                Error::from_uring_result(result, "Read operation failed")
            );
        }
        
        return static_cast<std::size_t>(result);
    }
    
    /// @brief Get queue depth
    [[nodiscard]] constexpr QueueDepth queue_depth() const noexcept {
        return queue_depth_;
    }
    
private:
    IoUringFileReader(IoUringGuard ring, QueueDepth depth) noexcept
        : ring_(std::move(ring)), queue_depth_(depth) {}
    
    IoUringGuard ring_;
    QueueDepth queue_depth_;
};

} // namespace io_uring_examples::v2
