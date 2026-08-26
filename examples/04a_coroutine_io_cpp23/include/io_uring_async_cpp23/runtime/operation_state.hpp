#pragma once

#include <coroutine>

namespace io_uring_async_cpp23 {

struct OperationState {
    enum class Status {
        Created,
        Pending,
        Completed,
    };

    explicit OperationState(std::coroutine_handle<> awaiting) noexcept
        : continuation(awaiting) {}

    std::coroutine_handle<> continuation;
    int result = 0;
    Status status = Status::Created;
};

} // namespace io_uring_async_cpp23
