#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace io_uring_async_cpp23;

Task<void> echo_bytes(
    IoUringExecutor& executor,
    TcpListener& listener,
    std::size_t expected,
    bool& succeeded) {
    auto accepted = co_await listener.accept();
    if (!accepted) {
        co_return;
    }

    TcpStream stream(executor, accepted.value());
    std::array<std::byte, 4096> buffer{};
    std::size_t total = 0;
    while (total < expected) {
        auto received = co_await stream.receive(buffer);
        if (!received || received.value() == 0) {
            co_return;
        }
        auto sent = co_await stream.send_all(
            std::span<const std::byte>(buffer.data(), received.value()));
        if (!sent || sent.value() != received.value()) {
            co_return;
        }
        total += received.value();
    }
    succeeded = total == expected;
}

int connect_loopback(AddressFamily family, std::uint16_t port) {
    const int domain = family == AddressFamily::IPv4 ? AF_INET : AF_INET6;
    const int fd = ::socket(domain, SOCK_STREAM | SOCK_CLOEXEC, 0);
    assert(fd >= 0);
    if (family == AddressFamily::IPv4) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        assert(::connect(fd, reinterpret_cast<sockaddr*>(&address),
                         sizeof(address)) == 0);
    } else {
        sockaddr_in6 address{};
        address.sin6_family = AF_INET6;
        address.sin6_port = htons(port);
        address.sin6_addr = in6addr_loopback;
        assert(::connect(fd, reinterpret_cast<sockaddr*>(&address),
                         sizeof(address)) == 0);
    }
    return fd;
}

void send_and_receive_exact(int fd, std::span<const std::byte> payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const auto count = ::send(fd, payload.data() + sent,
                                  payload.size() - sent, MSG_NOSIGNAL);
        assert(count > 0);
        sent += static_cast<std::size_t>(count);
    }

    std::vector<std::byte> echoed(payload.size());
    std::size_t received = 0;
    while (received < echoed.size()) {
        const auto count = ::recv(fd, echoed.data() + received,
                                  echoed.size() - received, 0);
        assert(count > 0);
        received += static_cast<std::size_t>(count);
    }
    assert(std::memcmp(echoed.data(), payload.data(), payload.size()) == 0);
}

void echo_round_trip(AddressFamily family, std::size_t payload_size) {
    auto executor = IoUringExecutor::create(64);
    assert(executor.has_value());
    auto& runtime = **executor;
    auto listener = TcpListener::bind(runtime, 0, 128, family);
    assert(listener.has_value());
    assert(listener->port() != 0);
    assert(listener->address_family() == family);

    bool server_ok = false;
    runtime.spawn(echo_bytes(runtime, *listener, payload_size, server_ok));
    std::vector<std::byte> payload(payload_size, std::byte{0x3c});
    std::thread client([&] {
        const int fd = connect_loopback(family, listener->port());
        constexpr std::size_t chunk_size = 4096;
        std::size_t offset = 0;
        while (offset < payload.size()) {
            const auto count = std::min(chunk_size, payload.size() - offset);
            send_and_receive_exact(fd, std::span(payload).subspan(offset, count));
            offset += count;
        }
        ::close(fd);
    });

    assert(runtime.run().has_value());
    client.join();
    assert(server_ok);
}

int main() {
    echo_round_trip(AddressFamily::IPv4, 2U * 1024U * 1024U);
    echo_round_trip(AddressFamily::IPv6, 64U * 1024U);
    return 0;
}
