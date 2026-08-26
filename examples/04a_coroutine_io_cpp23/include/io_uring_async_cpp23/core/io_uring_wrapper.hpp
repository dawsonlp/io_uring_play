/**
 * @file io_uring_wrapper.hpp
 * @brief RAII wrapper for io_uring with C++23 std::expected
 * 
 * Provides type-safe RAII management of io_uring resources.
 * C++23 version uses std::expected instead of custom Result<T,E>.
 */

#pragma once

#include "error.hpp"
#include <expected>
#include <liburing.h>
#include <memory>
#include <span>

namespace io_uring_async_cpp23 {

/**
 * @brief RAII wrapper for io_uring
 * 
 * Manages the lifecycle of an io_uring instance. Automatically initializes
 * and tears down the io_uring on construction/destruction.
 * 
 * C++23 improvement: Uses std::expected instead of custom Result type.
 */
class IoUring {
public:
    /**
     * @brief Create and initialize an io_uring instance
     * 
     * @param queue_depth Number of entries in submission/completion queues
     * @return std::expected containing IoUring or Error
     */
    [[nodiscard]] static std::expected<IoUring, Error> create(
        unsigned queue_depth = 256) noexcept 
    {
        io_uring ring;
        int ret = io_uring_queue_init(queue_depth, &ring, 0);
        
        if (ret < 0) {
            return std::unexpected(Error::from_errno(-ret));
        }
        
        return IoUring(ring);
    }
    
    /**
     * @brief Destructor - automatically tears down io_uring
     */
    ~IoUring() {
        if (initialized_) {
            io_uring_queue_exit(&ring_);
        }
    }
    
    // Move-only semantics
    IoUring(const IoUring&) = delete;
    IoUring& operator=(const IoUring&) = delete;
    
    IoUring(IoUring&& other) noexcept 
        : ring_(other.ring_)
        , initialized_(other.initialized_)
    {
        other.initialized_ = false;
    }
    
    IoUring& operator=(IoUring&& other) noexcept {
        if (this != &other) {
            if (initialized_) {
                io_uring_queue_exit(&ring_);
            }
            ring_ = other.ring_;
            initialized_ = other.initialized_;
            other.initialized_ = false;
        }
        return *this;
    }
    
    /**
     * @brief Get a submission queue entry for preparing an operation
     * 
     * @return std::expected containing SQE pointer or Error
     */
    [[nodiscard]] std::expected<io_uring_sqe*, Error> get_sqe() noexcept {
        io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (!sqe) {
            return std::unexpected(Error(Error::Code::QueueFull,
                "Failed to get SQE - submission queue full"));
        }
        return sqe;
    }
    
    /**
     * @brief Submit pending operations to the kernel
     * 
     * @return std::expected containing number submitted or Error
     */
    [[nodiscard]] std::expected<int, Error> submit() noexcept {
        int ret = io_uring_submit(&ring_);
        if (ret < 0) {
            return std::unexpected(Error::from_errno(-ret));
        }
        return ret;
    }
    
    /**
     * @brief Wait for a completion queue entry
     * 
     * @param cqe_ptr Pointer to store the CQE pointer
     * @return std::expected containing void or Error
     */
    [[nodiscard]] std::expected<void, Error> wait_cqe(
        io_uring_cqe** cqe_ptr) noexcept 
    {
        int ret = io_uring_wait_cqe(&ring_, cqe_ptr);
        if (ret < 0) {
            return std::unexpected(Error::from_errno(-ret));
        }
        return {};
    }
    
    /**
     * @brief Peek for a completion queue entry (non-blocking)
     * 
     * @param cqe_ptr Pointer to store the CQE pointer
     * @return std::expected containing bool (true if CQE available) or Error
     */
    [[nodiscard]] std::expected<bool, Error> peek_cqe(
        io_uring_cqe** cqe_ptr) noexcept 
    {
        int ret = io_uring_peek_cqe(&ring_, cqe_ptr);
        if (ret == 0) {
            return true;  // CQE available
        } else if (ret == -EAGAIN) {
            return false; // No CQE available (not an error)
        } else {
            return std::unexpected(Error::from_errno(-ret));
        }
    }
    
    /**
     * @brief Mark a CQE as seen (consumed)
     * 
     * @param cqe The CQE to mark as seen
     */
    void cqe_seen(io_uring_cqe* cqe) noexcept {
        io_uring_cqe_seen(&ring_, cqe);
    }
    
    /**
     * @brief Get the raw io_uring pointer (for advanced usage)
     */
    [[nodiscard]] io_uring* raw() noexcept {
        return &ring_;
    }
    
    /**
     * @brief Prepare a read operation
     * 
     * @param fd File descriptor
     * @param buffer Buffer to read into
     * @param offset File offset
     * @param user_data User data to associate with operation
     * @return std::expected containing void or Error
     */
    [[nodiscard]] std::expected<void, Error> prep_read(
        int fd, 
        std::span<std::byte> buffer,
        uint64_t offset,
        void* user_data) noexcept
    {
        auto sqe_result = get_sqe();
        if (!sqe_result) {
            return std::unexpected(sqe_result.error());
        }
        
        io_uring_prep_read(sqe_result.value(), fd, 
            buffer.data(), buffer.size(), offset);
        io_uring_sqe_set_data(sqe_result.value(), user_data);
        
        return {};
    }
    
    /**
     * @brief Prepare a write operation
     * 
     * @param fd File descriptor
     * @param buffer Buffer to write from
     * @param offset File offset
     * @param user_data User data to associate with operation
     * @return std::expected containing void or Error
     */
    [[nodiscard]] std::expected<void, Error> prep_write(
        int fd,
        std::span<const std::byte> buffer,
        uint64_t offset,
        void* user_data) noexcept
    {
        auto sqe_result = get_sqe();
        if (!sqe_result) {
            return std::unexpected(sqe_result.error());
        }
        
        io_uring_prep_write(sqe_result.value(), fd,
            buffer.data(), buffer.size(), offset);
        io_uring_sqe_set_data(sqe_result.value(), user_data);
        
        return {};
    }
    
    /**
     * @brief Prepare an accept operation
     * 
     * @param fd Socket file descriptor
     * @param addr Address structure pointer
     * @param addrlen Address length pointer
     * @param user_data User data to associate with operation
     * @return std::expected containing void or Error
     */
    [[nodiscard]] std::expected<void, Error> prep_accept(
        int fd,
        sockaddr* addr,
        socklen_t* addrlen,
        void* user_data) noexcept
    {
        auto sqe_result = get_sqe();
        if (!sqe_result) {
            return std::unexpected(sqe_result.error());
        }
        
        io_uring_prep_accept(sqe_result.value(), fd, addr, addrlen, 0);
        io_uring_sqe_set_data(sqe_result.value(), user_data);
        
        return {};
    }
    
    /**
     * @brief Prepare a receive operation
     * 
     * @param fd Socket file descriptor
     * @param buffer Buffer to receive into
     * @param user_data User data to associate with operation
     * @return std::expected containing void or Error
     */
    [[nodiscard]] std::expected<void, Error> prep_recv(
        int fd,
        std::span<std::byte> buffer,
        int flags,
        void* user_data) noexcept
    {
        auto sqe_result = get_sqe();
        if (!sqe_result) {
            return std::unexpected(sqe_result.error());
        }
        
        io_uring_prep_recv(sqe_result.value(), fd,
            buffer.data(), buffer.size(), flags);
        io_uring_sqe_set_data(sqe_result.value(), user_data);
        
        return {};
    }
    
    /**
     * @brief Prepare a send operation
     * 
     * @param fd Socket file descriptor
     * @param buffer Buffer to send from
     * @param user_data User data to associate with operation
     * @return std::expected containing void or Error
     */
    [[nodiscard]] std::expected<void, Error> prep_send(
        int fd,
        std::span<const std::byte> buffer,
        int flags,
        void* user_data) noexcept
    {
        auto sqe_result = get_sqe();
        if (!sqe_result) {
            return std::unexpected(sqe_result.error());
        }
        
        io_uring_prep_send(sqe_result.value(), fd,
            buffer.data(), buffer.size(), flags);
        io_uring_sqe_set_data(sqe_result.value(), user_data);
        
        return {};
    }
    
    /**
     * @brief Prepare a close operation
     * 
     * @param fd File descriptor to close
     * @param user_data User data to associate with operation
     * @return std::expected containing void or Error
     */
    [[nodiscard]] std::expected<void, Error> prep_close(
        int fd,
        void* user_data) noexcept
    {
        auto sqe_result = get_sqe();
        if (!sqe_result) {
            return std::unexpected(sqe_result.error());
        }
        
        io_uring_prep_close(sqe_result.value(), fd);
        io_uring_sqe_set_data(sqe_result.value(), user_data);
        
        return {};
    }
    
private:
    /**
     * @brief Private constructor (use create() factory)
     */
    explicit IoUring(io_uring ring) noexcept 
        : ring_(ring), initialized_(true) {}
    
    io_uring ring_;
    bool initialized_ = false;
};

} // namespace io_uring_async_cpp23
