/**
 * @file tcp_listener.hpp
 * @brief TCP listener for accepting connections (C++23)
 * 
 * Provides an async TCP listener that accepts connections using io_uring.
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <memory>

namespace io_uring_async_cpp23 {

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
 * auto listener = TcpListener::bind(executor, 8080);
 * if (!listener) {
 *     std::print(stderr, "Failed: {}\n", listener.error().description());
 *     co_return;
 * }
 * 
 * while (true) {
 *     auto client = co_await listener->accept();
 *     if (client) {
 *         // handle client connection
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
     * @return std::expected<TcpListener, Error>
     */
    [[nodiscard]] static std::expected<TcpListener, Error> bind(
        IoUringExecutor& executor,
        uint16_t port,
        int backlog = 128) noexcept 
    {
        int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd < 0) {
            return std::unexpected(Error::from_errno(errno));
        }
        
        // Allow address reuse
        int opt = 1;
        if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            int err = errno;
            ::close(fd);
            return std::unexpected(Error::from_errno(err));
        }
        
        // Bind to the port
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            int err = errno;
            ::close(fd);
            return std::unexpected(Error::from_errno(err));
        }
        
        // Start listening
        if (::listen(fd, backlog) < 0) {
            int err = errno;
            ::close(fd);
            return std::unexpected(Error::from_errno(err));
        }

        if (port == 0) {
            socklen_t addr_length = sizeof(addr);
            if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr),
                              &addr_length) < 0) {
                int err = errno;
                ::close(fd);
                return std::unexpected(Error::from_errno(err));
            }
            port = ntohs(addr.sin_port);
        }
        
        try {
            return TcpListener(executor, fd, port);
        } catch (const std::bad_alloc&) {
            return std::unexpected(Error(
                Error::Code::OutOfMemory,
                "Failed to allocate listener descriptor state"));
        }
    }
    
    // Move-only semantics
    TcpListener(TcpListener&& other) noexcept
        : executor_(other.executor_)
        , descriptor_(std::move(other.descriptor_))
        , port_(other.port_) 
    {}
    
    TcpListener& operator=(TcpListener&& other) noexcept {
        if (this != &other) {
            close();
            executor_ = other.executor_;
            descriptor_ = std::move(other.descriptor_);
            port_ = other.port_;
        }
        return *this;
    }
    
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;
    
    ~TcpListener() {
        close();
    }
    
    /**
     * @brief Accept a new connection
     * 
     * Suspends the coroutine until a client connects.
     * 
     * @return AcceptAwaitable that yields std::expected<int, Error>
     */
    [[nodiscard]] AcceptAwaitable accept() {
        return AcceptAwaitable(*executor_, descriptor_);
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
        return descriptor_ ? descriptor_->fd() : -1;
    }
    
    /**
     * @brief Release this listener's descriptor ownership
     *
     * The descriptor closes after the last in-flight accept releases it.
     */
    void close() noexcept {
        descriptor_.reset();
    }
    
private:
    TcpListener(IoUringExecutor& executor, int fd, uint16_t port)
        : executor_(&executor)
        , descriptor_(adopt_descriptor(fd))
        , port_(port) {}
    
    IoUringExecutor* executor_;
    std::shared_ptr<DescriptorState> descriptor_;
    uint16_t port_;
};

} // namespace io_uring_async_cpp23
