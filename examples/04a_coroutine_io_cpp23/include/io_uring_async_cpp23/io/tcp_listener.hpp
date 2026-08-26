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

enum class AddressFamily { IPv4, IPv6 };

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
     * @param family IPv4 by default; IPv6 listeners are explicitly v6-only
     * @return std::expected<TcpListener, Error>
     */
    [[nodiscard]] static std::expected<TcpListener, Error> bind(
        IoUringExecutor& executor,
        uint16_t port,
        int backlog = 128,
        AddressFamily family = AddressFamily::IPv4) noexcept
    {
        if (backlog <= 0) {
            return std::unexpected(Error(
                Error::Code::InvalidArgument,
                "Listen backlog must be positive"));
        }
        const int domain = family == AddressFamily::IPv4 ? AF_INET : AF_INET6;
        int fd = ::socket(domain, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
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
        
        sockaddr_in ipv4{};
        sockaddr_in6 ipv6{};
        sockaddr* bind_address = nullptr;
        socklen_t address_length = 0;
        if (family == AddressFamily::IPv4) {
            ipv4.sin_family = AF_INET;
            ipv4.sin_addr.s_addr = INADDR_ANY;
            ipv4.sin_port = htons(port);
            bind_address = reinterpret_cast<sockaddr*>(&ipv4);
            address_length = sizeof(ipv4);
        } else {
            int ipv6_only = 1;
            if (::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
                             &ipv6_only, sizeof(ipv6_only)) < 0) {
                int err = errno;
                ::close(fd);
                return std::unexpected(Error::from_errno(err));
            }
            ipv6.sin6_family = AF_INET6;
            ipv6.sin6_addr = in6addr_any;
            ipv6.sin6_port = htons(port);
            bind_address = reinterpret_cast<sockaddr*>(&ipv6);
            address_length = sizeof(ipv6);
        }

        if (::bind(fd, bind_address, address_length) < 0) {
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
            if (::getsockname(fd, bind_address,
                              &address_length) < 0) {
                int err = errno;
                ::close(fd);
                return std::unexpected(Error::from_errno(err));
            }
            if (family == AddressFamily::IPv4) {
                port = ntohs(ipv4.sin_port);
            } else {
                port = ntohs(ipv6.sin6_port);
            }
        }
        
        try {
            return TcpListener(executor, fd, port, family);
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
        , family_(other.family_)
    {}
    
    TcpListener& operator=(TcpListener&& other) noexcept {
        if (this != &other) {
            close();
            executor_ = other.executor_;
            descriptor_ = std::move(other.descriptor_);
            port_ = other.port_;
            family_ = other.family_;
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

    [[nodiscard]] AddressFamily address_family() const noexcept {
        return family_;
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
    TcpListener(IoUringExecutor& executor, int fd, uint16_t port,
                AddressFamily family)
        : executor_(&executor)
        , descriptor_(adopt_descriptor(fd))
        , port_(port)
        , family_(family) {}
    
    IoUringExecutor* executor_;
    std::shared_ptr<DescriptorState> descriptor_;
    uint16_t port_;
    AddressFamily family_;
};

} // namespace io_uring_async_cpp23
