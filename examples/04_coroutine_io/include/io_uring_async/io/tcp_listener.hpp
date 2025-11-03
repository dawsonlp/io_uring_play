/**
 * @file tcp_listener.hpp
 * @brief TCP listener for accepting connections
 * 
 * Provides an async TCP listener that accepts connections using io_uring.
 */

#pragma once

#include "../core/result.hpp"
#include "../core/error.hpp"
#include "../runtime/awaitable.hpp"
#include "../runtime/executor.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

namespace io_uring_async {

// Forward declaration
class TcpStream;

/**
 * @brief TCP listener for accepting incoming connections
 * 
 * TcpListener provides an async interface for accepting TCP connections.
 * It wraps a listening socket and provides coroutine-based accept().
 * 
 * Example:
 * @code
 * auto listener_result = TcpListener::bind(executor, 8080);
 * if (!listener_result) {
 *     // Handle error
 *     return;
 * }
 * 
 * auto listener = std::move(listener_result.value());
 * 
 * while (true) {
 *     auto client_result = co_await listener.accept();
 *     if (client_result) {
 *         // Handle client connection
 *         handle_client(std::move(client_result.value()));
 *     }
 * }
 * @endcode
 */
class TcpListener {
public:
    /**
     * @brief Bind to a port and start listening
     * 
     * Creates a TCP socket, binds it to the specified port, and starts listening.
     * 
     * @param executor The executor to use for async operations
     * @param port The port to listen on
     * @param backlog The listen backlog (default 128)
     * @return Result<TcpListener, Error>
     */
    [[nodiscard]] static Result<TcpListener, Error> bind(
        IoUringExecutor& executor,
        uint16_t port,
        int backlog = 128) noexcept;
    
    /**
     * @brief Move constructor
     */
    TcpListener(TcpListener&& other) noexcept
        : executor_(other.executor_)
        , fd_(other.fd_)
        , port_(other.port_) 
    {
        other.fd_ = -1;
    }
    
    /**
     * @brief Move assignment
     */
    TcpListener& operator=(TcpListener&& other) noexcept {
        if (this != &other) {
            close();
            executor_ = other.executor_;
            fd_ = other.fd_;
            port_ = other.port_;
            other.fd_ = -1;
        }
        return *this;
    }
    
    // Delete copy operations
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;
    
    /**
     * @brief Destructor - closes the socket
     */
    ~TcpListener() {
        close();
    }
    
    /**
     * @brief Accept a new connection
     * 
     * Suspends the coroutine until a client connects.
     * 
     * @return Awaitable that yields Result<TcpStream, Error>
     */
    [[nodiscard]] auto accept() -> AcceptAwaitable {
        return AcceptAwaitable(*executor_, fd_);
    }
    
    /**
     * @brief Get the port number
     */
    [[nodiscard]] uint16_t port() const noexcept {
        return port_;
    }
    
    /**
     * @brief Get the socket file descriptor
     */
    [[nodiscard]] int fd() const noexcept {
        return fd_;
    }
    
    /**
     * @brief Close the listener
     */
    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    
private:
    /**
     * @brief Private constructor (use bind() factory)
     */
    TcpListener(IoUringExecutor& executor, int fd, uint16_t port) noexcept
        : executor_(&executor)
        , fd_(fd)
        , port_(port) {}
    
    IoUringExecutor* executor_;  ///< Executor for async operations
    int fd_;                      ///< Socket file descriptor
    uint16_t port_;              ///< Port number
};

} // namespace io_uring_async
