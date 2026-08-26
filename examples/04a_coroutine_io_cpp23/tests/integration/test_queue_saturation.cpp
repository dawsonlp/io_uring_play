#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <cassert>
#include <optional>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

using namespace io_uring_async_cpp23;

Task<void> receive_once(IoUringExecutor& executor, int fd,
                        std::optional<Error>& error) {
    TcpStream stream(executor, fd);
    std::array<std::byte, 1> buffer{};
    auto result = co_await stream.receive(buffer);
    if (!result) {
        error = result.error();
    }
}

int main() {
    std::array<int, 2> first{};
    std::array<int, 2> second{};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, first.data()) == 0);
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, second.data()) == 0);

    auto executor = IoUringExecutor::create(1);
    assert(executor.has_value());

    std::optional<Error> first_error;
    std::optional<Error> second_error;
    executor->spawn(receive_once(*executor, first[0], first_error));
    executor->spawn(receive_once(*executor, second[0], second_error));

    std::jthread sender([fd = first[1]] {
        const std::byte marker{0x2a};
        assert(::send(fd, &marker, 1, 0) == 1);
        ::close(fd);
    });

    assert(executor->run().has_value());
    sender.join();
    ::close(second[1]);

    assert(!first_error.has_value());
    assert(second_error.has_value());
    assert(second_error->code() == Error::Code::QueueFull);
    return 0;
}
