#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <atomic>
#include <cassert>
#include <expected>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace io_uring_async_cpp23;

Task<void> send_complete_payload(
    TcpStream& stream,
    std::span<const std::byte> payload,
    std::expected<std::size_t, Error>& outcome) {
    outcome = co_await stream.send_all(payload);
}

Task<void> observe_eof(
    TcpStream& stream,
    std::expected<std::size_t, Error>& outcome) {
    std::array<std::byte, 16> buffer{};
    outcome = co_await stream.receive(buffer);
}

Task<void> observe_reset(
    TcpStream& stream,
    Error::Code& receive_outcome,
    Error::Code& send_outcome) {
    std::array<std::byte, 16> buffer{};
    auto received = co_await stream.receive(buffer);
    receive_outcome = received ? Error::Code::Success : received.error().code();
    const std::array<std::byte, 1> byte{std::byte{0x7f}};
    auto sent = co_await stream.send(byte);
    send_outcome = sent ? Error::Code::Success : sent.error().code();
}

std::array<int, 2> connected_tcp_pair() {
    const int listener = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    assert(listener >= 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(::bind(listener, reinterpret_cast<sockaddr*>(&address),
                  sizeof(address)) == 0);
    assert(::listen(listener, 1) == 0);
    socklen_t length = sizeof(address);
    assert(::getsockname(listener, reinterpret_cast<sockaddr*>(&address),
                         &length) == 0);

    const int client = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    assert(client >= 0);
    assert(::connect(client, reinterpret_cast<sockaddr*>(&address),
                     sizeof(address)) == 0);
    const int server = ::accept4(listener, nullptr, nullptr, SOCK_CLOEXEC);
    assert(server >= 0);
    ::close(listener);
    return {server, client};
}

void partial_send_eventually_delivers_all() {
    std::array<int, 2> sockets{};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);
    int send_buffer = 4096;
    assert(::setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF,
                        &send_buffer, sizeof(send_buffer)) == 0);

    constexpr std::size_t payload_size = 2U * 1024U * 1024U;
    std::vector<std::byte> payload(payload_size, std::byte{0x5a});
    std::atomic_size_t received = 0;
    std::thread reader([&] {
        std::array<std::byte, 1024> chunk{};
        while (received.load() < payload.size()) {
            const auto count = ::recv(sockets[1], chunk.data(), chunk.size(), 0);
            assert(count > 0);
            received.fetch_add(static_cast<std::size_t>(count));
        }
    });

    auto executor = IoUringExecutor::create(16);
    assert(executor.has_value());
    auto& runtime = **executor;
    TcpStream stream(runtime, sockets[0]);
    std::expected<std::size_t, Error> outcome =
        std::unexpected(Error(Error::Code::Unknown));
    runtime.spawn(send_complete_payload(stream, payload, outcome));
    assert(runtime.run().has_value());
    reader.join();

    assert(outcome.has_value() && outcome.value() == payload.size());
    assert(received.load() == payload.size());
    assert(runtime.test_operation_completion_count() > 1);
    ::close(sockets[1]);
}

void orderly_close_is_eof() {
    std::array<int, 2> sockets{};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);
    assert(::shutdown(sockets[1], SHUT_WR) == 0);

    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;
    TcpStream stream(runtime, sockets[0]);
    std::expected<std::size_t, Error> outcome =
        std::unexpected(Error(Error::Code::Unknown));
    runtime.spawn(observe_eof(stream, outcome));
    assert(runtime.run().has_value());
    assert(outcome.has_value() && outcome.value() == 0);
    ::close(sockets[1]);
}

void reset_is_connection_error_without_sigpipe() {
    auto sockets = connected_tcp_pair();
    linger reset{1, 0};
    assert(::setsockopt(sockets[1], SOL_SOCKET, SO_LINGER,
                        &reset, sizeof(reset)) == 0);
    ::close(sockets[1]);

    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;
    TcpStream stream(runtime, sockets[0]);
    auto receive_outcome = Error::Code::Unknown;
    auto send_outcome = Error::Code::Unknown;
    runtime.spawn(observe_reset(stream, receive_outcome, send_outcome));
    assert(runtime.run().has_value());
    assert(receive_outcome == Error::Code::ConnectionClosed);
    assert(send_outcome == Error::Code::ConnectionClosed);
}

int main() {
    partial_send_eventually_delivers_all();
    orderly_close_is_eof();
    reset_is_connection_error_without_sigpipe();
    return 0;
}
