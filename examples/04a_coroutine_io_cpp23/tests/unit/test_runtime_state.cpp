#include <io_uring_async_cpp23/runtime/executor.hpp>

#include <cassert>
#include <cerrno>
#include <coroutine>
#include <cstdint>

using namespace io_uring_async_cpp23;

int main() {
    auto executor = IoUringExecutor::create_for_testing();
    assert(executor.has_value());
    auto& runtime = **executor;

    io_uring_cqe unknown_cqe{};
    unknown_cqe.user_data = 1;
    const auto unknown = runtime.test_process_completion(&unknown_cqe);
    assert(!unknown.has_value());
    assert(unknown.error().code() == Error::Code::InvalidArgument);

    auto* state = runtime.create_operation(std::noop_coroutine());
    runtime.mark_operation_pending(state);
    io_uring_cqe valid_cqe{};
    valid_cqe.user_data = reinterpret_cast<std::uint64_t>(state);
    valid_cqe.res = 42;
    assert(runtime.test_process_completion(&valid_cqe).has_value());
    const auto duplicate = runtime.test_process_completion(&valid_cqe);
    assert(!duplicate.has_value());
    assert(duplicate.error().code() == Error::Code::InvalidArgument);
    const auto result = runtime.take_operation_result(state);
    assert(result.has_value() && *result == 42);

    auto* completion_first = runtime.create_operation(std::noop_coroutine());
    runtime.mark_operation_pending(completion_first);
    auto* late_cancel = runtime.test_create_cancellation(completion_first);
    io_uring_cqe operation_cqe{};
    operation_cqe.user_data = reinterpret_cast<std::uint64_t>(completion_first);
    operation_cqe.res = 7;
    assert(runtime.test_process_completion(&operation_cqe).has_value());
    io_uring_cqe late_cancel_cqe{};
    late_cancel_cqe.user_data = reinterpret_cast<std::uint64_t>(late_cancel);
    late_cancel_cqe.res = -ENOENT;
    assert(runtime.test_process_completion(&late_cancel_cqe).has_value());
    assert(runtime.take_operation_result(completion_first) == 7);

    auto* failed = runtime.create_operation(std::noop_coroutine());
    runtime.mark_operation_pending(failed);
    runtime.test_fail_next_submit(
        Error(Error::Code::PermissionDenied, "scripted submission failure"));
    const auto submission = runtime.test_submit_operations();
    assert(!submission.has_value());
    assert(submission.error().code() == Error::Code::PermissionDenied);
    const auto failed_result = runtime.take_operation_result(failed);
    assert(!failed_result.has_value());
    assert(failed_result.error().message() == "scripted submission failure");
    return 0;
}
