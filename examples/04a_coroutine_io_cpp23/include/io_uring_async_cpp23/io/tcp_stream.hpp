/**
 * @file tcp_stream.hpp
 * @brief TCP stream for bidirectional communication (C++23)
 * 
 * Provides an async TCP stream for reading and writing data.
 * 
 * C++23 improvements:
 * - std::expected instead of custom Result<T,E>
 */

#pragma once

#include "../core/error.hpp"
#include "../runtime/awaitable.hpp"
#include "../runtime/executor.hpp"
#include "../core/descriptor_state.hpp"
#include <expected>
#include <sys/socket.h>
#include <unistd.h>
#include <span>
#include <cstddef>
#include <memory>

namespace io_uring_async_cpp23 {

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
 *         if (!send_result) break;
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
    TcpStream(IoUringExecutor& executor, int fd)
        : executor_(&executor)
        , descriptor_(adopt_descriptor(fd)) {}
    
    // Move-only semantics
    TcpStream(TcpStream&& other) noexcept
        : executor_(other.executor_)
        , descriptor_(std::move(other.descriptor_)) {}
    
    TcpStream& operator=(TcpStream&& other) noexcept {
        if (this != &other) {
            close();
            executor_ = other.executor_;
            descriptor_ = std::move(other.descriptor_);
        }
        return *this;
    }
    
    TcpStream(const TcpStream&) = delete;
    TcpStream& operator=(const TcpStream&) = delete;
    
    ~TcpStream() {
        close();
    }
    
    /**
     * @brief Send data on the stream
     * 
     * Suspends until the data is sent (or an error occurs).
     * 
     * @param buffer The data to send
     * @return SendAwaitable that yields std::expected<size_t, Error>
     */
    [[nodiscard]] SendAwaitable send(std::span<const std::byte> buffer) {
        return SendAwaitable(*executor_, descriptor_, buffer);
    }
    
    /**
     * @brief Receive data from the stream
     * 
     * Suspends until data is received (or an error occurs).
     * Returns 0 bytes if the connection was closed gracefully.
     * 
     * @param buffer Buffer to receive into
     * @return RecvAwaitable that yields std::expected<size_t, Error>
     */
    [[nodiscard]] RecvAwaitable receive(std::span<std::byte> buffer) {
        return RecvAwaitable(*executor_, descriptor_, buffer);
    }
    
    /**
     * @brief Shutdown the write side of the connection
     * 
     * @return std::expected<void, Error>
     */
    [[nodiscard]] std::expected<void, Error> shutdown_write() noexcept {
        if (!descriptor_ || ::shutdown(descriptor_->fd(), SHUT_WR) < 0) {
            return std::unexpected(Error::from_errno(errno));
        }
        return {};
    }
    
    /**
     * @brief Shutdown the read side of the connection
     * 
     * @return std::expected<void, Error>
     */
    [[nodiscard]] std::expected<void, Error> shutdown_read() noexcept {
        if (!descriptor_ || ::shutdown(descriptor_->fd(), SHUT_RD) < 0) {
            return std::unexpected(Error::from_errno(errno));
        }
        return {};
    }
    
    /**
     * @brief Shutdown both sides of the connection
     * 
     * @return std::expected<void, Error>
     */
    [[nodiscard]] std::expected<void, Error> shutdown() noexcept {
        if (!descriptor_ || ::shutdown(descriptor_->fd(), SHUT_RDWR) < 0) {
            return std::unexpected(Error::from_errno(errno));
        }
        return {};
    }
    
    /**
     * @brief Get the socket file descriptor
     */
    [[nodiscard]] int fd() const noexcept {
        return descriptor_ ? descriptor_->fd() : -1;
    }
    
    /**
     * @brief Check if the stream is valid
     */
    [[nodiscard]] bool is_open() const noexcept {
        return static_cast<bool>(descriptor_);
    }
    
    /**
     * @brief Release this stream's descriptor ownership
     *
     * The descriptor closes after the last in-flight awaiter releases it.
     */
    void close() noexcept {
        descriptor_.reset();
    }
    
private:
    IoUringExecutor* executor_;
    std::shared_ptr<DescriptorState> descriptor_;
};

} // namespace io_uring_async_cpp23
