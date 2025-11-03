/**
 * @file tcp_listener.cpp
 * @brief Implementation of TcpListener
 */

#include "../../include/io_uring_async/io/tcp_listener.hpp"
#include "../../include/io_uring_async/io/tcp_stream.hpp"
#include <fcntl.h>

namespace io_uring_async {

Result<TcpListener, Error> TcpListener::bind(
    IoUringExecutor& executor,
    uint16_t port,
    int backlog) noexcept 
{
    // Create socket
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return Unexpected(Error::from_errno("socket"));
    }
    
    // Set SO_REUSEADDR to allow quick restarts
    int reuse = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        ::close(fd);
        return Unexpected(Error::from_errno("setsockopt(SO_REUSEADDR)"));
    }
    
    // Set SO_REUSEPORT for load balancing across threads (if needed)
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        // SO_REUSEPORT is optional, continue without it
    }
    
    // Bind to address
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return Unexpected(Error::from_errno("bind"));
    }
    
    // Start listening
    if (::listen(fd, backlog) < 0) {
        ::close(fd);
        return Unexpected(Error::from_errno("listen"));
    }
    
    return TcpListener(executor, fd, port);
}

} // namespace io_uring_async
