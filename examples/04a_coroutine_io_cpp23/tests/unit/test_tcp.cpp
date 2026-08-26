#include <io_uring_async_cpp23/io/tcp_listener.hpp>

#include <cassert>

using namespace io_uring_async_cpp23;

int main() {
    auto executor = IoUringExecutor::create(64);
    assert(executor.has_value());
    auto& runtime = **executor;

    auto listener = TcpListener::bind(runtime, 0);
    assert(listener.has_value());
    assert(listener->fd() >= 0);
    return 0;
}
