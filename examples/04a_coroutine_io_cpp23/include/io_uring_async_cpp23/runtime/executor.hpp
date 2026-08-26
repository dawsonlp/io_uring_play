/**
 * @file executor.hpp
 * @brief Single-threaded io_uring coroutine executor.
 */

#pragma once

#include "../core/error.hpp"
#include "../core/io_uring_wrapper.hpp"
#include "../core/task.hpp"
#include "operation_state.hpp"

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_map>

namespace io_uring_async_cpp23 {

class AcceptAwaitable;
class ReadAwaitable;
class RecvAwaitable;
class SendAwaitable;
class WriteAwaitable;

/**
 * Owns the event loop, root tasks, and every in-flight operation state.
 * Except for request_stop(), the executor is confined to its creating thread.
 */
class IoUringExecutor {
public:
    ~IoUringExecutor();

    [[nodiscard]] static std::expected<std::unique_ptr<IoUringExecutor>, Error>
    create(unsigned queue_depth = 256) noexcept;

    void spawn(Task<void>&& task);
    [[nodiscard]] std::expected<void, Error> run();
    [[nodiscard]] std::expected<bool, Error> run_once();
    [[nodiscard]] bool accepts_work() const noexcept;
    void request_stop() noexcept;
    [[nodiscard]] std::expected<void, Error> shutdown();

    [[nodiscard]] OperationState* create_operation(
        std::coroutine_handle<> continuation);
    void mark_operation_pending(OperationState* state) noexcept;
    void discard_operation(OperationState* state) noexcept;
    [[nodiscard]] std::expected<int, Error> take_operation_result(
        OperationState* state);

#ifdef IO_URING_ASYNC_TESTING
    // Deterministic fault/completion seams used by the architecture tests.
    void test_fail_next_submit(Error error);
    [[nodiscard]] std::expected<void, Error> test_process_completion(
        io_uring_cqe* cqe);
    [[nodiscard]] CancellationState* test_create_cancellation(
        OperationState* target);
    [[nodiscard]] std::size_t test_operation_completion_count() const noexcept;
#endif

    IoUringExecutor(const IoUringExecutor&) = delete;
    IoUringExecutor& operator=(const IoUringExecutor&) = delete;
    IoUringExecutor(IoUringExecutor&&) = delete;
    IoUringExecutor& operator=(IoUringExecutor&&) = delete;

private:
    using TaskIterator = std::list<Task<void>>::iterator;

    explicit IoUringExecutor(IoUring&& ring, int wake_fd);

    void assert_owner_thread() const noexcept;
    [[nodiscard]] IoUring& ring() noexcept;
    [[nodiscard]] std::expected<void, Error> process_ready_tasks(
        std::size_t limit);
    [[nodiscard]] std::expected<int, Error> submit_pending();
    [[nodiscard]] std::expected<void, Error> submit_or_fail_operations();
    [[nodiscard]] std::expected<void, Error> prepare_cancellations();
    [[nodiscard]] bool has_user_operations() const noexcept;
    [[nodiscard]] std::expected<void, Error> arm_wake();
    void fail_all_operations(const Error& error);
    [[nodiscard]] std::expected<void, Error> process_completion(
        io_uring_cqe* cqe);
    [[nodiscard]] std::expected<void, Error> process_and_consume(
        io_uring_cqe* cqe);
    [[nodiscard]] std::expected<void, Error> wait_for_one_completion();
    [[nodiscard]] std::expected<void, Error> drain_available_completions();
    [[nodiscard]] std::expected<void, Error> remove_completed_tasks();

    friend class AcceptAwaitable;
    friend class ReadAwaitable;
    friend class RecvAwaitable;
    friend class SendAwaitable;
    friend class WriteAwaitable;

    IoUring ring_;
    std::list<Task<void>> active_tasks_;
    std::queue<TaskIterator> ready_queue_;
    std::unordered_map<OperationState*, std::unique_ptr<OperationState>> operations_;
    std::unordered_map<CancellationState*, std::unique_ptr<CancellationState>>
        cancellations_;
    std::thread::id owner_thread_;
    bool stopping_ = false;
    std::atomic_bool stop_requested_ = false;
    int wake_fd_ = -1;
    std::uint64_t wake_value_ = 0;
    OperationState* wake_operation_ = nullptr;
    std::optional<Error> next_submit_error_;
    std::size_t operation_completion_count_ = 0;
};

} // namespace io_uring_async_cpp23
