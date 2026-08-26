#pragma once

#include "../core/error.hpp"

#include <coroutine>
#include <optional>

namespace io_uring_async_cpp23 {

struct OperationState {
    enum class Status {
        Created,
        Pending,
        Completed,
        Failed,
    };

    explicit OperationState(std::coroutine_handle<> awaiting) noexcept
        : continuation(awaiting) {}

    std::coroutine_handle<> continuation;
    int result = 0;
    std::optional<Error> error;
    Status status = Status::Created;
    bool cancellation_requested = false;
    bool executor_wake = false;
};

struct CancellationState {
    explicit CancellationState(OperationState* target_operation) noexcept
        : target(target_operation) {}

    OperationState* target;
};

} // namespace io_uring_async_cpp23
