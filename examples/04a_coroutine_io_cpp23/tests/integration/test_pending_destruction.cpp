#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <cassert>

#include <sys/socket.h>
#include <unistd.h>

using namespace io_uring_async_cpp23;

Task<void> pending_receive(IoUringExecutor& executor, int fd) {
    TcpStream stream(executor, fd);
    std::array<std::byte, 32> buffer{};
    (void)co_await stream.receive(buffer);
}

int main() {
    std::array<int, 2> sockets{};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);

    {
        auto executor = IoUringExecutor::create(8);
        assert(executor.has_value());
        auto& runtime = **executor;
        runtime.spawn(pending_receive(runtime, sockets[0]));

        auto has_work = runtime.run_once();
        assert(has_work.has_value());
        assert(has_work.value());
        // Executor destruction must tear down the ring before destroying the
        // suspended coroutine frame and its receive buffer.
    }

    ::close(sockets[1]);
    return 0;
}
