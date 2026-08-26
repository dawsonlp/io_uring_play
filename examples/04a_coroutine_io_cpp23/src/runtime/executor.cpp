#ifndef IO_URING_ASYNC_TESTING
#define IO_URING_ASYNC_TESTING
#endif
#include "io_uring_async_cpp23/runtime/executor.hpp"

#include <cassert>
#include <cerrno>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <vector>

#include <sys/eventfd.h>
#include <unistd.h>

namespace io_uring_async_cpp23 {

IoUringExecutor::IoUringExecutor(IoUring&& ring, int wake_fd)
    : ring_(std::move(ring)), owner_thread_(std::this_thread::get_id()),
      wake_fd_(wake_fd) {}

IoUringExecutor::~IoUringExecutor() {
    assert_owner_thread();
    (void)shutdown();
    ring_.close();
    while (!ready_queue_.empty()) {
        ready_queue_.pop();
    }
    cancellations_.clear();
    operations_.clear();
    active_tasks_.clear();
    if (wake_fd_ >= 0) {
        ::close(wake_fd_);
    }
}

std::expected<std::unique_ptr<IoUringExecutor>, Error>
IoUringExecutor::create(unsigned queue_depth) noexcept {
    auto ring_result = IoUring::create(queue_depth);
    if (!ring_result) {
        return std::unexpected(ring_result.error());
    }

    const int wake_fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_fd < 0) {
        return std::unexpected(Error::from_errno(errno));
    }

    auto executor = std::unique_ptr<IoUringExecutor>(
        new (std::nothrow) IoUringExecutor(std::move(*ring_result), wake_fd));
    if (!executor) {
        ::close(wake_fd);
        return std::unexpected(
            Error(Error::Code::OutOfMemory, "Failed to allocate executor"));
    }
    return executor;
}

void IoUringExecutor::spawn(Task<void>&& task) {
    assert_owner_thread();
    if (stopping_) {
        throw std::logic_error("Cannot spawn work after executor shutdown begins");
    }
    if (task.valid()) {
        active_tasks_.push_back(std::move(task));
        ready_queue_.push(std::prev(active_tasks_.end()));
    }
}

std::expected<void, Error> IoUringExecutor::run() {
    assert_owner_thread();
    while (!ready_queue_.empty() || has_user_operations()) {
        if (stop_requested_.load(std::memory_order_acquire)) {
            return shutdown();
        }

        auto ready_result = process_ready_tasks(
            std::numeric_limits<std::size_t>::max());
        if (!ready_result) {
            return ready_result;
        }

        if (!has_user_operations()) {
            continue;
        }
        auto wake_result = arm_wake();
        if (!wake_result) {
            return wake_result;
        }
        auto submit_result = submit_or_fail_operations();
        if (!submit_result) {
            return submit_result;
        }
        auto wait_result = wait_for_one_completion();
        if (!wait_result) {
            return wait_result;
        }
        if (stop_requested_.load(std::memory_order_acquire)) {
            return shutdown();
        }
        auto drain_result = drain_available_completions();
        if (!drain_result) {
            return drain_result;
        }
        auto task_result = remove_completed_tasks();
        if (!task_result) {
            return task_result;
        }
    }
    return {};
}

std::expected<bool, Error> IoUringExecutor::run_once() {
    assert_owner_thread();
    if (stop_requested_.load(std::memory_order_acquire)) {
        auto shutdown_result = shutdown();
        if (!shutdown_result) {
            return std::unexpected(shutdown_result.error());
        }
        return false;
    }

    auto ready_result = process_ready_tasks(ready_queue_.size());
    if (!ready_result) {
        return std::unexpected(ready_result.error());
    }
    if (!operations_.empty()) {
        auto submit_result = submit_or_fail_operations();
        if (!submit_result) {
            return std::unexpected(submit_result.error());
        }
        auto drain_result = drain_available_completions();
        if (!drain_result) {
            return std::unexpected(drain_result.error());
        }
        auto task_result = remove_completed_tasks();
        if (!task_result) {
            return std::unexpected(task_result.error());
        }
    }
    return !ready_queue_.empty() || has_user_operations();
}

bool IoUringExecutor::accepts_work() const noexcept {
    assert_owner_thread();
    return !stopping_;
}

void IoUringExecutor::request_stop() noexcept {
    stop_requested_.store(true, std::memory_order_release);
    const std::uint64_t signal = 1;
    const auto written = ::write(wake_fd_, &signal, sizeof(signal));
    (void)written;
}

std::expected<void, Error> IoUringExecutor::shutdown() {
    assert_owner_thread();
    stopping_ = true;

    auto ready_result = process_ready_tasks(
        std::numeric_limits<std::size_t>::max());
    if (!ready_result) {
        return ready_result;
    }

    while (!operations_.empty() || !cancellations_.empty()) {
        auto cancellation_result = prepare_cancellations();
        if (!cancellation_result) {
            return cancellation_result;
        }
        auto submit_result = submit_pending();
        if (!submit_result) {
            return std::unexpected(submit_result.error());
        }
        auto wait_result = wait_for_one_completion();
        if (!wait_result) {
            return wait_result;
        }
        auto drain_result = drain_available_completions();
        if (!drain_result) {
            return drain_result;
        }
        auto task_result = remove_completed_tasks();
        if (!task_result) {
            return task_result;
        }
    }
    return {};
}

OperationState* IoUringExecutor::create_operation(
    std::coroutine_handle<> continuation) {
    assert_owner_thread();
    auto state = std::make_unique<OperationState>(continuation);
    auto* state_ptr = state.get();
    operations_.emplace(state_ptr, std::move(state));
    return state_ptr;
}

void IoUringExecutor::mark_operation_pending(OperationState* state) noexcept {
    assert_owner_thread();
    state->status = OperationState::Status::Pending;
}

void IoUringExecutor::discard_operation(OperationState* state) noexcept {
    assert_owner_thread();
    operations_.erase(state);
}

std::expected<int, Error> IoUringExecutor::take_operation_result(
    OperationState* state) {
    assert_owner_thread();
    auto operation = operations_.find(state);
    if (operation == operations_.end()) {
        return std::unexpected(
            Error(Error::Code::InvalidArgument, "Unknown operation result"));
    }
    if (operation->second->status == OperationState::Status::Failed) {
        auto error = std::move(*operation->second->error);
        operations_.erase(operation);
        return std::unexpected(std::move(error));
    }
    if (operation->second->status != OperationState::Status::Completed) {
        return std::unexpected(Error(
            Error::Code::InvalidArgument,
            "Operation result requested before completion"));
    }
    const int result = operation->second->result;
    operations_.erase(operation);
    return result;
}

void IoUringExecutor::assert_owner_thread() const noexcept {
    assert(owner_thread_ == std::this_thread::get_id() &&
           "IoUringExecutor used from a non-owner thread");
}

IoUring& IoUringExecutor::ring() noexcept {
    assert_owner_thread();
    return ring_;
}

std::expected<void, Error> IoUringExecutor::process_ready_tasks(
    std::size_t limit) {
    std::size_t processed = 0;
    while (!ready_queue_.empty() && processed < limit) {
        auto task = ready_queue_.front();
        ready_queue_.pop();
        task->resume();
        ++processed;
    }
    return remove_completed_tasks();
}

std::expected<int, Error> IoUringExecutor::submit_pending() {
    if (next_submit_error_) {
        auto error = std::move(*next_submit_error_);
        next_submit_error_.reset();
        return std::unexpected(std::move(error));
    }
    return ring_.submit();
}

std::expected<void, Error> IoUringExecutor::submit_or_fail_operations() {
    auto submit_result = submit_pending();
    if (submit_result) {
        return {};
    }
    const auto error = submit_result.error();
    fail_all_operations(error);
    (void)remove_completed_tasks();
    return std::unexpected(error);
}

std::expected<void, Error> IoUringExecutor::prepare_cancellations() {
    for (auto& [state_ptr, state] : operations_) {
        if (state->status != OperationState::Status::Pending ||
            state->cancellation_requested) {
            continue;
        }
        auto cancellation = std::make_unique<CancellationState>(state_ptr);
        auto* cancellation_ptr = cancellation.get();
        auto preparation = ring_.prep_cancel(state_ptr, cancellation_ptr);
        if (!preparation) {
            if (preparation.error().code() == Error::Code::QueueFull) {
                return {};
            }
            return std::unexpected(preparation.error());
        }
        state->cancellation_requested = true;
        cancellations_.emplace(cancellation_ptr, std::move(cancellation));
    }
    return {};
}

bool IoUringExecutor::has_user_operations() const noexcept {
    return operations_.size() > (wake_operation_ ? 1U : 0U);
}

std::expected<void, Error> IoUringExecutor::arm_wake() {
    if (wake_operation_ || stop_requested_.load(std::memory_order_acquire)) {
        return {};
    }
    auto* state = create_operation(std::noop_coroutine());
    state->executor_wake = true;
    auto buffer = std::as_writable_bytes(std::span{&wake_value_, 1U});
    auto preparation = ring_.prep_read(wake_fd_, buffer, 0, state);
    if (!preparation && preparation.error().code() == Error::Code::QueueFull) {
        discard_operation(state);
        auto submit_result = submit_pending();
        if (!submit_result) {
            return std::unexpected(submit_result.error());
        }
        state = create_operation(std::noop_coroutine());
        state->executor_wake = true;
        preparation = ring_.prep_read(wake_fd_, buffer, 0, state);
    }
    if (!preparation) {
        discard_operation(state);
        return std::unexpected(preparation.error());
    }
    mark_operation_pending(state);
    wake_operation_ = state;
    return {};
}

void IoUringExecutor::fail_all_operations(const Error& error) {
    std::vector<std::coroutine_handle<>> continuations;
    continuations.reserve(operations_.size());
    for (auto& [state_ptr, state] : operations_) {
        (void)state_ptr;
        if (state->status == OperationState::Status::Pending) {
            state->status = OperationState::Status::Failed;
            state->error = error;
            continuations.push_back(state->continuation);
        }
    }
    for (auto continuation : continuations) {
        if (continuation) {
            continuation.resume();
        }
    }
}

std::expected<void, Error> IoUringExecutor::process_completion(io_uring_cqe* cqe) {
    auto* state = static_cast<OperationState*>(io_uring_cqe_get_data(cqe));
    auto operation = operations_.find(state);
    if (operation != operations_.end()) {
        if (operation->second->status != OperationState::Status::Pending) {
            return std::unexpected(Error(
                Error::Code::InvalidArgument,
                "Duplicate or stale io_uring operation completion"));
        }
        state->result = cqe->res;
        state->status = OperationState::Status::Completed;
        if (state->executor_wake) {
            operations_.erase(operation);
            wake_operation_ = nullptr;
            return {};
        }
        ++operation_completion_count_;
        auto continuation = state->continuation;
        if (continuation) {
            continuation.resume();
        }
        return {};
    }

    auto* cancellation_state =
        static_cast<CancellationState*>(io_uring_cqe_get_data(cqe));
    auto cancellation = cancellations_.find(cancellation_state);
    if (cancellation == cancellations_.end()) {
        return std::unexpected(Error(
            Error::Code::InvalidArgument,
            "Unknown, duplicate, or stale io_uring completion"));
    }
    cancellations_.erase(cancellation);
    if (cqe->res < 0 && cqe->res != -ENOENT && cqe->res != -EALREADY) {
        return std::unexpected(Error::from_errno(-cqe->res));
    }
    return {};
}

std::expected<void, Error> IoUringExecutor::process_and_consume(
    io_uring_cqe* cqe) {
    auto result = process_completion(cqe);
    ring_.cqe_seen(cqe);
    return result;
}

std::expected<void, Error> IoUringExecutor::wait_for_one_completion() {
    io_uring_cqe* cqe = nullptr;
    auto wait_result = ring_.wait_cqe(&cqe);
    if (!wait_result) {
        return std::unexpected(wait_result.error());
    }
    return process_and_consume(cqe);
}

std::expected<void, Error> IoUringExecutor::drain_available_completions() {
    while (true) {
        io_uring_cqe* cqe = nullptr;
        auto peek_result = ring_.peek_cqe(&cqe);
        if (!peek_result) {
            return std::unexpected(peek_result.error());
        }
        if (!*peek_result) {
            return {};
        }
        auto completion_result = process_and_consume(cqe);
        if (!completion_result) {
            return completion_result;
        }
    }
}

std::expected<void, Error> IoUringExecutor::remove_completed_tasks() {
    for (auto task = active_tasks_.begin(); task != active_tasks_.end();) {
        if (!task->done()) {
            ++task;
            continue;
        }
        try {
            task->rethrow_if_failed();
        } catch (const std::exception& exception) {
            task = active_tasks_.erase(task);
            return std::unexpected(
                Error(Error::Code::TaskFailed, exception.what()));
        } catch (...) {
            task = active_tasks_.erase(task);
            return std::unexpected(Error(
                Error::Code::TaskFailed, "Unknown task exception"));
        }
        task = active_tasks_.erase(task);
    }
    return {};
}

void IoUringExecutor::test_fail_next_submit(Error error) {
    next_submit_error_ = std::move(error);
}

std::expected<std::unique_ptr<IoUringExecutor>, Error>
IoUringExecutor::create_for_testing() noexcept {
    const int wake_fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_fd < 0) {
        return std::unexpected(Error::from_errno(errno));
    }
    auto executor = std::unique_ptr<IoUringExecutor>(
        new (std::nothrow) IoUringExecutor(IoUring::testing_stub(), wake_fd));
    if (!executor) {
        ::close(wake_fd);
        return std::unexpected(
            Error(Error::Code::OutOfMemory, "Failed to allocate test executor"));
    }
    return executor;
}

std::expected<void, Error> IoUringExecutor::test_submit_operations() {
    return submit_or_fail_operations();
}

std::expected<void, Error> IoUringExecutor::test_process_completion(
    io_uring_cqe* cqe) {
    return process_completion(cqe);
}

CancellationState* IoUringExecutor::test_create_cancellation(
    OperationState* target) {
    auto cancellation = std::make_unique<CancellationState>(target);
    auto* cancellation_ptr = cancellation.get();
    target->cancellation_requested = true;
    cancellations_.emplace(cancellation_ptr, std::move(cancellation));
    return cancellation_ptr;
}

std::size_t IoUringExecutor::test_operation_completion_count() const noexcept {
    return operation_completion_count_;
}

} // namespace io_uring_async_cpp23
