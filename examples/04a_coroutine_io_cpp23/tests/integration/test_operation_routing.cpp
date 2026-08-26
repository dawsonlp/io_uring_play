#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

using namespace io_uring_async_cpp23;

Task<void> receive_marker(IoUringExecutor& executor, int fd,
                          std::byte expected, bool& matched,
                          unsigned& resume_count) {
    TcpStream stream(executor, fd);
    std::array<std::byte, 1> buffer{};
    auto result = co_await stream.receive(buffer);
    ++resume_count;
    matched = result && result.value() == 1 && buffer[0] == expected;
}

int main() {
    constexpr std::size_t operation_count = 32;
    std::array<std::array<int, 2>, operation_count> sockets{};
    std::array<bool, operation_count> matched{};
    std::array<unsigned, operation_count> resume_counts{};

    auto executor = IoUringExecutor::create(64);
    assert(executor.has_value());

    for (std::size_t index = 0; index < operation_count; ++index) {
        assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets[index].data()) == 0);
        executor->spawn(receive_marker(
            *executor, sockets[index][0], static_cast<std::byte>(index),
            matched[index], resume_counts[index]));
    }

    std::jthread sender([&] {
        for (std::size_t index = operation_count; index-- > 0;) {
            const auto marker = static_cast<std::byte>(index);
            assert(::send(sockets[index][1], &marker, 1, 0) == 1);
            ::close(sockets[index][1]);
        }
    });

    assert(executor->run().has_value());
    sender.join();

    for (std::size_t index = 0; index < operation_count; ++index) {
        assert(matched[index]);
        assert(resume_counts[index] == 1);
    }
    return 0;
}
