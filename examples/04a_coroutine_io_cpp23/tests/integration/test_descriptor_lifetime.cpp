#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <cassert>
#include <cerrno>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace io_uring_async_cpp23;

Task<void> receive_once(TcpStream& stream, Error::Code& outcome) {
    std::array<std::byte, 32> buffer{};
    auto result = co_await stream.receive(buffer);
    outcome = result ? Error::Code::Success : result.error().code();
}

Task<void> send_once(TcpStream& stream, Error::Code& outcome) {
    const std::array<std::byte, 1> buffer{std::byte{0x2a}};
    auto result = co_await stream.send(buffer);
    outcome = result ? Error::Code::Success : result.error().code();
}

Task<void> accept_once(TcpListener& listener, Error::Code& outcome) {
    auto result = co_await listener.accept();
    outcome = result ? Error::Code::Success : result.error().code();
    if (result) {
        ::close(result.value());
    }
}

void close_stream_with_pending_receive_and_reuse_fd() {
    std::array<int, 2> sockets{};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);

    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;
    TcpStream stream(runtime, sockets[0]);
    const int original_fd = stream.fd();
    auto outcome = Error::Code::Unknown;
    runtime.spawn(receive_once(stream, outcome));
    auto has_work = runtime.run_once();
    assert(has_work.has_value() && has_work.value());

    stream.close();
    assert(!stream.is_open());
    assert(::fcntl(original_fd, F_GETFD) >= 0);
    assert(runtime.shutdown().has_value());
    assert(outcome == Error::Code::Cancelled);
    assert(::fcntl(original_fd, F_GETFD) < 0 && errno == EBADF);

    const int reused_fd = ::open("/dev/null", O_RDONLY);
    assert(reused_fd == original_fd);
    ::close(reused_fd);
    ::close(sockets[1]);
}

void close_stream_with_pending_send() {
    std::array<int, 2> sockets{};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);

    std::array<std::byte, 4096> fill{};
    while (::send(sockets[0], fill.data(), fill.size(), MSG_DONTWAIT) > 0) {
    }
    assert(errno == EAGAIN || errno == EWOULDBLOCK);

    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;
    TcpStream stream(runtime, sockets[0]);
    const int original_fd = stream.fd();
    auto outcome = Error::Code::Unknown;
    runtime.spawn(send_once(stream, outcome));
    auto has_work = runtime.run_once();
    assert(has_work.has_value() && has_work.value());

    stream.close();
    assert(::fcntl(original_fd, F_GETFD) >= 0);
    assert(runtime.shutdown().has_value());
    assert(outcome == Error::Code::Cancelled);
    assert(::fcntl(original_fd, F_GETFD) < 0 && errno == EBADF);
    ::close(sockets[1]);
}

void close_listener_with_pending_accept() {
    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;
    auto listener_result = TcpListener::bind(runtime, 0);
    assert(listener_result.has_value());
    auto listener = std::move(listener_result.value());
    const int original_fd = listener.fd();
    auto outcome = Error::Code::Unknown;
    runtime.spawn(accept_once(listener, outcome));
    auto has_work = runtime.run_once();
    assert(has_work.has_value() && has_work.value());

    listener.close();
    assert(::fcntl(original_fd, F_GETFD) >= 0);
    assert(runtime.shutdown().has_value());
    assert(outcome == Error::Code::Cancelled);
    assert(::fcntl(original_fd, F_GETFD) < 0 && errno == EBADF);
}

int main() {
    close_stream_with_pending_receive_and_reuse_fd();
    close_stream_with_pending_send();
    close_listener_with_pending_accept();
    return 0;
}
