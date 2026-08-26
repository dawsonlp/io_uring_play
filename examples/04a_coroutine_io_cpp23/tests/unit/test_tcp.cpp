#include <io_uring_async_cpp23/io/tcp_listener.hpp>
#include <io_uring_async_cpp23/io/tcp_stream.hpp>

#include <cassert>
#include <stdexcept>

using namespace io_uring_async_cpp23;

int main() {
    auto executor = IoUringExecutor::create(64);
    assert(executor.has_value());
    auto& runtime = **executor;

    auto listener = TcpListener::bind(runtime, 0);
    assert(listener.has_value());
    assert(listener->fd() >= 0);

    auto invalid_backlog = TcpListener::bind(runtime, 0, 0);
    assert(!invalid_backlog.has_value());
    assert(invalid_backlog.error().code() == Error::Code::InvalidArgument);

    bool invalid_stream_rejected = false;
    try {
        TcpStream invalid(runtime, -1);
    } catch (const std::invalid_argument&) {
        invalid_stream_rejected = true;
    }
    assert(invalid_stream_rejected);
    return 0;
}
