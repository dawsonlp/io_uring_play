#include <io_uring_async_cpp23/runtime/executor.hpp>

#include <cassert>
#include <stdexcept>
#include <type_traits>

using namespace io_uring_async_cpp23;

Task<void> set_value(int& value) {
    value = 42;
    co_return;
}

Task<void> throwing_child() {
    throw std::runtime_error("root task regression");
    co_return;
}

Task<void> await_throwing_child() {
    co_await throwing_child();
}

int main() {
    static_assert(!std::is_move_constructible_v<IoUringExecutor>);
    static_assert(!std::is_move_assignable_v<IoUringExecutor>);

    auto executor = IoUringExecutor::create(64);
    assert(executor.has_value());
    auto& runtime = **executor;
    assert(runtime.run().has_value());

    int value = 0;
    runtime.spawn(set_value(value));
    assert(runtime.run().has_value());
    assert(value == 42);

    runtime.spawn(await_throwing_child());
    auto failure = runtime.run();
    assert(!failure.has_value());
    assert(failure.error().code() == Error::Code::TaskFailed);
    assert(failure.error().message() == "root task regression");

#ifdef IO_URING_ASYNC_TESTING
    io_uring_cqe unknown_cqe{};
    unknown_cqe.user_data = 1;
    auto unknown = runtime.test_process_completion(&unknown_cqe);
    assert(!unknown.has_value());
    assert(unknown.error().code() == Error::Code::InvalidArgument);

    auto* state = runtime.create_operation(std::noop_coroutine());
    runtime.mark_operation_pending(state);
    io_uring_cqe valid_cqe{};
    valid_cqe.user_data = reinterpret_cast<uint64_t>(state);
    assert(runtime.test_process_completion(&valid_cqe).has_value());
    auto duplicate = runtime.test_process_completion(&valid_cqe);
    assert(!duplicate.has_value());
    assert(duplicate.error().code() == Error::Code::InvalidArgument);
    runtime.discard_operation(state);
#endif
    return 0;
}
