/**
 * @file tcp_stream.hpp
 * @brief TCP stream for bidirectional communication
 * 
 * Provides an async TCP stream for reading and writing data.
 */

#pragma once

#include "../core/result.hpp"
#include "../core/error.hpp"
#include "../runtime/awaitable.hpp"
#include "../runtime/executor.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <span>
#include <cstddef>

namespace io_uring_async {

/**
 * @brief TCP stream for bidirectional communication
 * 
 * TcpStream wraps a connected TCP socket and provides async send/receive.
 * It is move-only and automatically closes the socket when destroyed.
 * 
 * Example:
 * @code
 * Task<void> echo_client(TcpStream stream) {
 *     std::array<std::byte, 1024> buffer;
 *     
 *     while (true) {
 *         auto recv_result = co_await stream.receive(buffer);
 *         if (!recv_result || recv_result.value() == 0) {
 *             break;  // Connection closed or error
 *         }
 *         
 *         size_t n = recv_result.value();
 *         auto send_result = co_await stream.send(
 *             std::span{buffer.data(), n}
 *         );
 *         
 *         if (!send_result) {
 *             break;  // Send error
 *         }
 *     }
 * }
 * @endcode
 */
class TcpStream {
public:
    /**
     * @brief Construct from file descriptor
     * 
     * @param executor The executor for async operations
     * @param fd The connected socket file descriptor
     */
    TcpStream(IoUringExecutor& executor, int fd) noexcept
        : executor_(&executor)
        , fd_(fd) {}
    
    /**
     * @brief Move constructor
     */
    TcpStream(TcpStream&& other) noexcept
        : executor_(other.executor_)
        , fd_(other.fd_)
    {
        other.fd_ = -1;
    }
    
    /**
     * @brief Move assignment
     */
    TcpStream& operator=(TcpStream&& other) noexcept {
        if (this != &other) {
            close();
            executor_ = other.executor_;
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    
    // Delete copy operations
    TcpStream(const TcpStream&) = delete;
    TcpStream& operator=(const TcpStream&) = delete;
    
    /**
     * @brief Destructor - closes the socket
     */
    ~TcpStream() {
        close();
    }
    
    /**
     * @brief Send data on the stream
     * 
     * Suspends until the data is sent (or an error occurs).
     * 
     * @param buffer The data to send
     * @return Awaitable that yields Result<size_t, Error> (bytes sent)
     */
    [[nodiscard]] auto send(std::span<const std::byte> buffer) -> SendAwaitable {
        return SendAwaitable(*executor_, fd_, buffer);
    }
    
    /**
     * @brief Receive data from the stream
     * 
     * Suspends until data is received (or an error occurs).
     * Returns 0 if the connection was closed gracefully.
     * 
     * @param buffer Buffer to receive into
     * @return Awaitable that yields Result<size_t, Error> (bytes received)
     */
    [[nodiscard]] auto receive(std::span<std::byte> buffer) -> RecvAwaitable {
        return RecvAwaitable(*executor_, fd_, buffer);
    }
    
    /**
     * @brief Shutdown the write side of the connection
     * 
     * @return Result<void, Error>
     */
    [[nodiscard]] Result<void, Error> shutdown_write() noexcept {
        if (::shutdown(fd_, SHUT_WR) < 0) {
            return Unexpected(Error::from_errno("shutdown"));
        }
        return {};
    }
    
    /**
     * @brief Shutdown the read side of the connection
     * 
     * @return Result<void, Error>
     */
    [[nodiscard]] Result<void, Error> shutdown_read() noexcept {
        if (::shutdown(fd_, SHUT_RD) < 0) {
            return Unexpected(Error::from_errno("shutdown"));
        }
        return {};
    }
    
    /**
     * @brief Shutdown both sides of the connection
     * 
     * @return Result<void, Error>
     */
    [[nodiscard]] Result<void, Error> shutdown() noexcept {
        if (::shutdown(fd_, SHUT_RDWR) < 0) {
            return Unexpected(Error::from_errno("shutdown"));
        }
        return {};
    }
    
    /**
     * @brief Get the socket file descriptor
     */
    [[nodiscard]] int fd() const noexcept {
        return fd_;
    }
    
    /**
     * @brief Check if the stream is valid
     */
    [[nodiscard]] bool is_open() const noexcept {
        return fd_ >= 0;
    }
    
    /**
     * @brief Close the stream
     */
    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    
private:
    IoUringExecutor* executor_;  ///< Executor for async operations
    int fd_;                      ///< Socket file descriptor
};

} // namespace io_uring_async
