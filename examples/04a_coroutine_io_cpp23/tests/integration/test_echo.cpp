#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <cassert>
#include <cstring>
#include <string_view>
#include <thread>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace io_uring_async_cpp23;

Task<void> echo_once(IoUringExecutor& executor, TcpListener& listener,
                     bool& server_ok) {
    auto accepted = co_await listener.accept();
    if (!accepted) {
        co_return;
    }

    TcpStream stream(executor, accepted.value());
    std::array<std::byte, 64> buffer{};
    auto received = co_await stream.receive(buffer);
    if (!received || received.value() == 0) {
        co_return;
    }

    auto sent = co_await stream.send(
        std::span<const std::byte>(buffer.data(), received.value()));
    server_ok = sent && sent.value() == received.value();
}

int main() {
    constexpr std::string_view payload = "io_uring integration";

    auto executor = IoUringExecutor::create(64);
    assert(executor.has_value());
    auto listener = TcpListener::bind(*executor, 0);
    assert(listener.has_value());
    assert(listener->port() != 0);

    bool server_ok = false;
    executor->spawn(echo_once(*executor, *listener, server_ok));

    bool client_ok = false;
    std::jthread client([port = listener->port(), payload, &client_ok] {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        assert(fd >= 0);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        assert(::connect(fd, reinterpret_cast<sockaddr*>(&address),
                         sizeof(address)) == 0);

        assert(::send(fd, payload.data(), payload.size(), 0) ==
               static_cast<ssize_t>(payload.size()));

        std::array<char, 64> reply{};
        const auto received = ::recv(fd, reply.data(), reply.size(), 0);
        client_ok = received == static_cast<ssize_t>(payload.size()) &&
                    std::memcmp(reply.data(), payload.data(), payload.size()) == 0;
        ::close(fd);
    });

    const auto run_result = executor->run();
    client.join();

    assert(run_result.has_value());
    assert(server_ok);
    assert(client_ok);
    return 0;
}
