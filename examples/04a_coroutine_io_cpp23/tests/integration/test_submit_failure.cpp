#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <cassert>
#include <optional>

#include <sys/socket.h>
#include <unistd.h>

using namespace io_uring_async_cpp23;

Task<void> wait_for_receive(IoUringExecutor& executor, int fd,
                            std::optional<Error>& result_error) {
    TcpStream stream(executor, fd);
    std::array<std::byte, 1> buffer{};
    auto result = co_await stream.receive(buffer);
    if (!result) {
        result_error = result.error();
    }
}

int main() {
    std::array<int, 2> sockets{};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);

    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;

    std::optional<Error> operation_error;
    runtime.spawn(wait_for_receive(runtime, sockets[0], operation_error));
    runtime.test_fail_next_submit(
        Error(Error::Code::PermissionDenied, "injected submit failure"));

    auto run_result = runtime.run();
    ::close(sockets[1]);

    assert(!run_result.has_value());
    assert(run_result.error().code() == Error::Code::PermissionDenied);
    assert(operation_error.has_value());
    assert(operation_error->code() == Error::Code::PermissionDenied);
    assert(operation_error->message() == "injected submit failure");
    assert(runtime.run().has_value());
    return 0;
}
