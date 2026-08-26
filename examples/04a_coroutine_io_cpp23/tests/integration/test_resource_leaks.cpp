#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <filesystem>

#include <sys/socket.h>
#include <unistd.h>

using namespace io_uring_async_cpp23;

std::size_t open_descriptor_count() {
    std::size_t count = 0;
    for ([[maybe_unused]] const auto& entry :
         std::filesystem::directory_iterator("/proc/self/fd")) {
        ++count;
    }
    return count;
}

int main() {
    const auto baseline = open_descriptor_count();
    {
        auto executor = IoUringExecutor::create(16);
        assert(executor.has_value());

        std::array<int, 2> sockets{};
        assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);
        TcpStream stream(**executor, sockets[0]);
        assert(::close(sockets[1]) == 0);

        auto listener = TcpListener::bind(**executor, 0);
        assert(listener.has_value());
        assert(listener->port() != 0);
    }
    assert(open_descriptor_count() == baseline);
    return 0;
}
