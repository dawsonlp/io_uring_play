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

// ============================================================================
// IoUringAwaitable<T> base implementations
// ============================================================================

template<typename T>
void IoUringAwaitable<T>::await_suspend(std::coroutine_handle<> awaiter) {
    operation_id_ = executor_->allocate_operation_id();
    
    if (!submit_operation()) {
        // If submission fails, resume immediately with an error
        raw_result_ = -ENOMEM;
        awaiter.resume();
        return;
    }
    
    // Register with executor so we get resumed on completion
    executor_->register_operation(operation_id_, awaiter);
}

template<typename T>
std::expected<T, Error> IoUringAwaitable<T>::await_resume() {
    // Get the result that the executor stored for us
    int res = executor_->get_operation_result(operation_id_);
    executor_->clear_operation_result(operation_id_);
    return process_result(res);
}

// Explicit template instantiations for the types we use
template class IoUringAwaitable<size_t>;
template class IoUringAwaitable<int>;
template class IoUringAwaitable<void>;

// ============================================================================
// ReadAwaitable
// ============================================================================

bool ReadAwaitable::submit_operation() {
    auto result = executor_->ring().prep_read(
        fd_, buffer_, offset_, operation_id_);
    return result.has_value();
}

std::expected<size_t, Error> ReadAwaitable::process_result(int res) {
    if (res < 0) {
        return std::unexpected(Error::from_errno(-res));
    }
    return static_cast<size_t>(res);
}

// ============================================================================
// WriteAwaitable
// ============================================================================

bool WriteAwaitable::submit_operation() {
    auto result = executor_->ring().prep_write(
        fd_, buffer_, offset_, operation_id_);
    return result.has_value();
}

std::expected<size_t, Error> WriteAwaitable::process_result(int res) {
    if (res < 0) {
        return std::unexpected(Error::from_errno(-res));
    }
    return static_cast<size_t>(res);
}

// ============================================================================
// AcceptAwaitable
// ============================================================================

bool AcceptAwaitable::submit_operation() {
    auto result = executor_->ring().prep_accept(
        fd_, 
        reinterpret_cast<sockaddr*>(&addr_), 
        &addrlen_,
        operation_id_);
    return result.has_value();
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

bool SendAwaitable::submit_operation() {
    auto result = executor_->ring().prep_send(
        fd_, buffer_, operation_id_);
    return result.has_value();
}

std::expected<size_t, Error> SendAwaitable::process_result(int res) {
    if (res < 0) {
        return std::unexpected(Error::from_errno(-res));
    }
    return static_cast<size_t>(res);
}

// ============================================================================
// RecvAwaitable
// ============================================================================

bool RecvAwaitable::submit_operation() {
    auto result = executor_->ring().prep_recv(
        fd_, buffer_, operation_id_);
    return result.has_value();
}

std::expected<size_t, Error> RecvAwaitable::process_result(int res) {
    if (res < 0) {
        return std::unexpected(Error::from_errno(-res));
    }
    return static_cast<size_t>(res);
}

// ============================================================================
// CloseAwaitable
// ============================================================================

bool CloseAwaitable::submit_operation() {
    auto result = executor_->ring().prep_close(fd_, operation_id_);
    return result.has_value();
}

std::expected<void, Error> CloseAwaitable::process_result(int res) {
    if (res < 0) {
        return std::unexpected(Error::from_errno(-res));
    }
    return {};
}

} // namespace io_uring_async_cpp23
