#include <io_uring_async_cpp23/io/tcp_listener.hpp>

#include <cassert>

using namespace io_uring_async_cpp23;

int main() {
    auto executor = IoUringExecutor::create(64);
    assert(executor.has_value());

    auto listener = TcpListener::bind(*executor, 0);
    assert(listener.has_value());
    assert(listener->fd() >= 0);
    return 0;
}
