#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <cassert>

using namespace io_uring_async_cpp23;

struct Outcomes {
    std::array<bool, 5> failed{};
};

Task<void> exercise_failures(IoUringExecutor& executor, Outcomes& outcomes) {
    std::array<std::byte, 8> buffer{};

    auto read = co_await ReadAwaitable(executor, -1, buffer, 0);
    outcomes.failed[0] = !read && read.error().code() == Error::Code::InvalidArgument;

    auto write = co_await WriteAwaitable(executor, -1, buffer, 0);
    outcomes.failed[1] = !write && write.error().code() == Error::Code::InvalidArgument;

    auto accept = co_await AcceptAwaitable(executor, -1);
    outcomes.failed[2] = !accept && accept.error().code() == Error::Code::InvalidArgument;

    auto send = co_await SendAwaitable(executor, -1, buffer);
    outcomes.failed[3] = !send && send.error().code() == Error::Code::InvalidArgument;

    auto receive = co_await RecvAwaitable(executor, -1, buffer);
    outcomes.failed[4] = !receive && receive.error().code() == Error::Code::InvalidArgument;
}

int main() {
    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;

    Outcomes outcomes;
    runtime.spawn(exercise_failures(runtime, outcomes));
    assert(runtime.run().has_value());
    for (bool failed : outcomes.failed) {
        assert(failed);
    }
    return 0;
}
