/**
 * @file awaitable.cpp
 * @brief Implementation of awaitable operations (C++23)
 * 
 * Implements the submit_operation() and process_result() methods for
 * each concrete awaitable type, as well as the base await_suspend/resume.
 */

#include <io_uring_async_cpp23/runtime/awaitable.hpp>
#include <io_uring_async_cpp23/runtime/executor.hpp>

namespace io_uring_async_cpp23 {

namespace {

std::expected<std::size_t, Error> byte_count_result(int result) {
    if (result < 0) {
        return std::unexpected(Error::from_errno(-result));
    }
    return static_cast<std::size_t>(result);
}

} // namespace

// ============================================================================
// IoUringAwaitable<T> base implementations
// ============================================================================

template<typename T>
bool IoUringAwaitable<T>::await_suspend(std::coroutine_handle<> awaiter) {
    if (!executor_->accepts_work()) {
        preparation_error_ = Error(
            Error::Code::Cancelled,
            "Executor shutdown is in progress");
        return false;
    }

    operation_state_ = executor_->create_operation(awaiter);

    auto preparation = submit_operation();
    if (!preparation) {
        preparation_error_ = std::move(preparation.error());
        executor_->discard_operation(operation_state_);
        operation_state_ = nullptr;
        return false;
    }

    executor_->mark_operation_pending(operation_state_);
    return true;
}

template<typename T>
std::expected<T, Error> IoUringAwaitable<T>::await_resume() {
    if (preparation_error_) {
        return std::unexpected(std::move(*preparation_error_));
    }

    auto result = executor_->take_operation_result(operation_state_);
    if (!result) {
        return std::unexpected(std::move(result.error()));
    }
    return process_result(result.value());
}

// Explicit template instantiations for the types we use
template class IoUringAwaitable<size_t>;
template class IoUringAwaitable<int>;
template class IoUringAwaitable<void>;

// ============================================================================
// ReadAwaitable
// ============================================================================

std::expected<void, Error> ReadAwaitable::submit_operation() {
    if (fd_ < 0) {
        return std::unexpected(Error(
            Error::Code::InvalidArgument, "Invalid read descriptor"));
    }
    return executor_->ring().prep_read(
        fd_, buffer_, offset_, operation_state_);
}

std::expected<size_t, Error> ReadAwaitable::process_result(int res) {
    return byte_count_result(res);
}

// ============================================================================
// WriteAwaitable
// ============================================================================

std::expected<void, Error> WriteAwaitable::submit_operation() {
    if (fd_ < 0) {
        return std::unexpected(Error(
            Error::Code::InvalidArgument, "Invalid write descriptor"));
    }
    return executor_->ring().prep_write(
        fd_, buffer_, offset_, operation_state_);
}

std::expected<size_t, Error> WriteAwaitable::process_result(int res) {
    return byte_count_result(res);
}

// ============================================================================
// AcceptAwaitable
// ============================================================================

std::expected<void, Error> AcceptAwaitable::submit_operation() {
    if (fd_ < 0) {
        return std::unexpected(Error(
            Error::Code::InvalidArgument, "Invalid accept descriptor"));
    }
    return executor_->ring().prep_accept(
        fd_, 
        reinterpret_cast<sockaddr*>(&addr_), 
        &addrlen_, flags_,
        operation_state_);
}

std::expected<int, Error> AcceptAwaitable::process_result(int res) {
    if (res < 0) {
        return std::unexpected(Error::from_errno(-res));
    }
    return res;  // File descriptor of accepted connection
}

// ============================================================================
// SendAwaitable
// ============================================================================

std::expected<void, Error> SendAwaitable::submit_operation() {
    if (fd_ < 0) {
        return std::unexpected(Error(
            Error::Code::InvalidArgument, "Invalid send descriptor"));
    }
    return executor_->ring().prep_send(
        fd_, buffer_, flags_, operation_state_);
}

std::expected<size_t, Error> SendAwaitable::process_result(int res) {
    return byte_count_result(res);
}

// ============================================================================
// RecvAwaitable
// ============================================================================

std::expected<void, Error> RecvAwaitable::submit_operation() {
    if (fd_ < 0) {
        return std::unexpected(Error(
            Error::Code::InvalidArgument, "Invalid receive descriptor"));
    }
    return executor_->ring().prep_recv(
        fd_, buffer_, flags_, operation_state_);
}

std::expected<size_t, Error> RecvAwaitable::process_result(int res) {
    return byte_count_result(res);
}

} // namespace io_uring_async_cpp23
