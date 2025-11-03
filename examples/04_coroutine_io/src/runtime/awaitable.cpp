/**
 * @file awaitable.cpp
 * @brief Implementation of io_uring awaitable types
 */

#include "../../include/io_uring_async/runtime/awaitable.hpp"
#include "../../include/io_uring_async/runtime/executor.hpp"
#include <sys/socket.h>

namespace io_uring_async {

// Base IoUringAwaitable implementation
template<typename T>
void IoUringAwaitable<T>::await_suspend(std::coroutine_handle<> awaiter) {
    // Allocate a unique operation ID
    operation_id_ = executor_->allocate_operation_id();
    
    // Register this operation with the executor
    executor_->register_operation(operation_id_, awaiter);
    
    // Submit the operation to io_uring
    if (!submit_operation()) {
        // Queue full - this shouldn't happen with proper sizing
        // For now, we'll proceed anyway and let the executor handle it
    }
}

template<typename T>
Result<T, Error> IoUringAwaitable<T>::await_resume() {
    // Get the operation result from the executor
    result_ = executor_->get_operation_result(operation_id_);
    
    // Clear the result from executor's storage
    executor_->clear_operation_result(operation_id_);
    
    // Process the result based on operation type
    return process_result(result_);
}

// Explicit template instantiations
template class IoUringAwaitable<size_t>;
template class IoUringAwaitable<int>;
template class IoUringAwaitable<void>;

// ReadAwaitable implementation
bool ReadAwaitable::submit_operation() {
    auto* sqe = executor_->ring().prep_read(fd_, buffer_, offset_, operation_id_);
    return sqe != nullptr;
}

Result<size_t, Error> ReadAwaitable::process_result(int res) {
    if (res < 0) {
        return Unexpected(Error::from_io_uring_result(res, "read"));
    }
    return static_cast<size_t>(res);
}

// WriteAwaitable implementation
bool WriteAwaitable::submit_operation() {
    auto* sqe = executor_->ring().prep_write(fd_, buffer_, offset_, operation_id_);
    return sqe != nullptr;
}

Result<size_t, Error> WriteAwaitable::process_result(int res) {
    if (res < 0) {
        return Unexpected(Error::from_io_uring_result(res, "write"));
    }
    return static_cast<size_t>(res);
}

// AcceptAwaitable implementation
bool AcceptAwaitable::submit_operation() {
    auto* sqe = executor_->ring().prep_accept(
        fd_,
        reinterpret_cast<sockaddr*>(&addr_),
        &addrlen_,
        operation_id_
    );
    return sqe != nullptr;
}

Result<int, Error> AcceptAwaitable::process_result(int res) {
    if (res < 0) {
        return Unexpected(Error::from_io_uring_result(res, "accept"));
    }
    return res;  // Return the new socket fd
}

// SendAwaitable implementation
bool SendAwaitable::submit_operation() {
    auto* sqe = executor_->ring().prep_send(fd_, buffer_, flags_, operation_id_);
    return sqe != nullptr;
}

Result<size_t, Error> SendAwaitable::process_result(int res) {
    if (res < 0) {
        return Unexpected(Error::from_io_uring_result(res, "send"));
    }
    return static_cast<size_t>(res);
}

// RecvAwaitable implementation
bool RecvAwaitable::submit_operation() {
    auto* sqe = executor_->ring().prep_recv(fd_, buffer_, flags_, operation_id_);
    return sqe != nullptr;
}

Result<size_t, Error> RecvAwaitable::process_result(int res) {
    if (res < 0) {
        return Unexpected(Error::from_io_uring_result(res, "recv"));
    }
    return static_cast<size_t>(res);
}

// CloseAwaitable implementation
bool CloseAwaitable::submit_operation() {
    auto* sqe = executor_->ring().prep_close(fd_, operation_id_);
    return sqe != nullptr;
}

Result<void, Error> CloseAwaitable::process_result(int res) {
    if (res < 0) {
        return Unexpected(Error::from_io_uring_result(res, "close"));
    }
    return {};  // Success
}

} // namespace io_uring_async
