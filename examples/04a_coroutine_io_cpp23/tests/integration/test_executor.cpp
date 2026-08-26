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

    auto* completion_first = runtime.create_operation(std::noop_coroutine());
    runtime.mark_operation_pending(completion_first);
    auto* late_cancel = runtime.test_create_cancellation(completion_first);
    io_uring_cqe operation_cqe{};
    operation_cqe.user_data = reinterpret_cast<uint64_t>(completion_first);
    operation_cqe.res = 7;
    assert(runtime.test_process_completion(&operation_cqe).has_value());
    io_uring_cqe late_cancel_cqe{};
    late_cancel_cqe.user_data = reinterpret_cast<uint64_t>(late_cancel);
    late_cancel_cqe.res = -ENOENT;
    assert(runtime.test_process_completion(&late_cancel_cqe).has_value());
    auto completion_result = runtime.take_operation_result(completion_first);
    assert(completion_result.has_value() && completion_result.value() == 7);

    auto* cancellation_first = runtime.create_operation(std::noop_coroutine());
    runtime.mark_operation_pending(cancellation_first);
    auto* winning_cancel = runtime.test_create_cancellation(cancellation_first);
    io_uring_cqe winning_cancel_cqe{};
    winning_cancel_cqe.user_data = reinterpret_cast<uint64_t>(winning_cancel);
    winning_cancel_cqe.res = 0;
    assert(runtime.test_process_completion(&winning_cancel_cqe).has_value());
    io_uring_cqe cancelled_operation_cqe{};
    cancelled_operation_cqe.user_data = reinterpret_cast<uint64_t>(cancellation_first);
    cancelled_operation_cqe.res = -ECANCELED;
    assert(runtime.test_process_completion(&cancelled_operation_cqe).has_value());
    auto cancellation_result = runtime.take_operation_result(cancellation_first);
    assert(cancellation_result.has_value());
    assert(cancellation_result.value() == -ECANCELED);
#endif
    return 0;
}
