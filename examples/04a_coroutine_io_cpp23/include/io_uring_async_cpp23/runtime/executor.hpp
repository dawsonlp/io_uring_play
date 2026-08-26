/**
 * @file executor.hpp
 * @brief IoUringExecutor - Event loop for async I/O operations (C++23)
 * 
 * The executor is the runtime that manages the event loop, schedules coroutines,
 * submits operations to io_uring, and processes completions to resume coroutines.
 * 
 * C++23 improvements over C++20 version:
 * - std::expected instead of custom Result<T,E>
 * - std::print for diagnostic output
 * 
 * This is a single-threaded executor designed for clarity and predictable performance.
 */

#pragma once

#include "../core/io_uring_wrapper.hpp"
#include "../core/task.hpp"
#include "../core/error.hpp"
#include "operation_state.hpp"
#include <expected>
#include <list>
#include <queue>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cassert>
#include <optional>
#include <thread>
#include <vector>
#include <stdexcept>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <span>
#include <sys/eventfd.h>
#include <unistd.h>
#include <new>

namespace io_uring_async_cpp23 {

/**
 * @brief IoUringExecutor - Single-threaded async I/O executor
 * 
 * The executor manages the lifecycle of async operations:
 * 1. Tasks are spawned and added to the ready queue
 * 2. The event loop runs, processing ready tasks
 * 3. When tasks await I/O, operations are submitted to io_uring
 * 4. Completions resume waiting tasks
 * 
 * Usage:
 * @code
 * auto executor = IoUringExecutor::create();
 * if (!executor) { return 1; }
 * auto& runtime = **executor;
 * runtime.spawn(my_async_task());
 * auto result = runtime.run();
 * @endcode
 */
class IoUringExecutor {
public:
    ~IoUringExecutor() {
        assert_owner_thread();

        // Prefer the graceful sequence. If cancellation or draining fails, ring
        // release remains the final kernel/resource lifetime safety boundary.
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

    /**
     * @brief Create an executor with the specified queue depth
     * 
     * @param queue_depth io_uring queue depth (default 256)
     * @return A stable, heap-owned executor or an initialization error
     */
    [[nodiscard]] static std::expected<std::unique_ptr<IoUringExecutor>, Error> create(
        unsigned queue_depth = 256) noexcept 
    {
        auto ring_result = IoUring::create(queue_depth);
        if (!ring_result) {
            return std::unexpected(ring_result.error());
        }
        
        const int wake_fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (wake_fd < 0) {
            return std::unexpected(Error::from_errno(errno));
        }

        auto executor = std::unique_ptr<IoUringExecutor>(
            new (std::nothrow) IoUringExecutor(
                std::move(ring_result.value()), wake_fd));
        if (!executor) {
            ::close(wake_fd);
            return std::unexpected(Error(
                Error::Code::OutOfMemory,
                "Failed to allocate executor"));
        }
        return executor;
    }
    
    /**
     * @brief Spawn a task on the executor
     * 
     * The task will be started and scheduled for execution.
     * 
     * @param task The task to spawn
     */
    void spawn(Task<void>&& task) {
        assert_owner_thread();
        if (stopping_) {
            throw std::logic_error("Cannot spawn work after executor shutdown begins");
        }
        if (task.valid()) {
            active_tasks_.push_back(std::move(task));
            ready_queue_.push(std::prev(active_tasks_.end()));
        }
    }
    
    /**
     * @brief Run the event loop until all tasks complete
     * 
     * This processes ready tasks and waits for I/O completions
     * until there are no more pending operations.
     * 
     * @return std::expected<void, Error>
     */
    [[nodiscard]] std::expected<void, Error> run() {
        assert_owner_thread();
        while (!ready_queue_.empty() || has_user_operations()) {
            if (stop_requested_.load(std::memory_order_acquire)) {
                return shutdown();
            }
            // Process all ready tasks
            while (!ready_queue_.empty()) {
                auto task = ready_queue_.front();
                ready_queue_.pop();
                task->resume();
            }

            auto task_result = remove_completed_tasks();
            if (!task_result) {
                return std::unexpected(task_result.error());
            }
            
            // If we have pending I/O operations, wait for completions
            if (!operations_.empty()) {
                auto wake_result = arm_wake();
                if (!wake_result) {
                    return std::unexpected(wake_result.error());
                }
                auto submit_result = submit_pending();
                if (!submit_result) {
                    const auto error = submit_result.error();
                    fail_all_operations(error);
                    (void)remove_completed_tasks();
                    return std::unexpected(error);
                }
                
                // Wait for at least one completion
                io_uring_cqe* cqe;
                auto wait_result = ring_.wait_cqe(&cqe);
                if (!wait_result) {
                    return std::unexpected(wait_result.error());
                }
                
                auto completion_result = process_completion(cqe);
                ring_.cqe_seen(cqe);
                if (!completion_result) {
                    return std::unexpected(completion_result.error());
                }

                if (stop_requested_.load(std::memory_order_acquire)) {
                    return shutdown();
                }
                
                // Drain any additional completions without blocking
                while (true) {
                    io_uring_cqe* peek_cqe;
                    auto peek_result = ring_.peek_cqe(&peek_cqe);
                    
                    if (!peek_result) {
                        return std::unexpected(peek_result.error());
                    }
                    
                    if (!peek_result.value()) {
                        break;  // No more CQEs available
                    }
                    
                    completion_result = process_completion(peek_cqe);
                    ring_.cqe_seen(peek_cqe);
                    if (!completion_result) {
                        return std::unexpected(completion_result.error());
                    }
                }

                task_result = remove_completed_tasks();
                if (!task_result) {
                    return std::unexpected(task_result.error());
                }
            }
        }
        
        return {};
    }
    
    /**
     * @brief Run one iteration of the event loop
     * 
     * Processes ready tasks and one batch of I/O completions.
     * Useful for integrating with other event loops.
     * 
     * @return std::expected<bool, Error> - true if more work remains
     */
    [[nodiscard]] std::expected<bool, Error> run_once() {
        assert_owner_thread();
        if (stop_requested_.load(std::memory_order_acquire)) {
            auto shutdown_result = shutdown();
            if (!shutdown_result) {
                return std::unexpected(shutdown_result.error());
            }
            return false;
        }
        // Process ready tasks
        size_t tasks_processed = 0;
        const size_t max_tasks = ready_queue_.size();
        
        while (!ready_queue_.empty() && tasks_processed < max_tasks) {
            auto task = ready_queue_.front();
            ready_queue_.pop();
            task->resume();
            tasks_processed++;
        }

        auto task_result = remove_completed_tasks();
        if (!task_result) {
            return std::unexpected(task_result.error());
        }
        
        // Check for I/O completions
        if (!operations_.empty()) {
            auto submit_result = submit_pending();
            if (!submit_result) {
                const auto error = submit_result.error();
                fail_all_operations(error);
                (void)remove_completed_tasks();
                return std::unexpected(error);
            }
            
            io_uring_cqe* cqe;
            auto peek_result = ring_.peek_cqe(&cqe);
            
            if (!peek_result) {
                return std::unexpected(peek_result.error());
            }
            
            if (peek_result.value()) {
                do {
                    auto completion_result = process_completion(cqe);
                    ring_.cqe_seen(cqe);
                    if (!completion_result) {
                        return std::unexpected(completion_result.error());
                    }
                    
                    peek_result = ring_.peek_cqe(&cqe);
                    if (!peek_result) {
                        return std::unexpected(peek_result.error());
                    }
                } while (peek_result.value());

                task_result = remove_completed_tasks();
                if (!task_result) {
                    return std::unexpected(task_result.error());
                }
            }
        }
        
        bool has_work = !ready_queue_.empty() || has_user_operations();
        return has_work;
    }
    
    /**
     * @brief Get the io_uring wrapper (for advanced usage)
     */
    [[nodiscard]] IoUring& ring() noexcept {
        assert_owner_thread();
        return ring_;
    }

    [[nodiscard]] bool accepts_work() const noexcept {
        assert_owner_thread();
        return !stopping_;
    }

    /**
     * @brief Thread-safe request that wakes a blocked run() and starts shutdown
     */
    void request_stop() noexcept {
        stop_requested_.store(true, std::memory_order_release);
        const std::uint64_t signal = 1;
        const auto written = ::write(wake_fd_, &signal, sizeof(signal));
        (void)written;
    }

    /**
     * @brief Stop accepting work, cancel pending I/O, and drain all completions
     */
    [[nodiscard]] std::expected<void, Error> shutdown() {
        assert_owner_thread();
        stopping_ = true;

        while (!ready_queue_.empty()) {
            auto task = ready_queue_.front();
            ready_queue_.pop();
            task->resume();
        }

        auto task_result = remove_completed_tasks();
        if (!task_result) {
            return std::unexpected(task_result.error());
        }

        while (!operations_.empty() || !cancellations_.empty()) {
            auto cancellation_result = prepare_cancellations();
            if (!cancellation_result) {
                return std::unexpected(cancellation_result.error());
            }

            auto submit_result = submit_pending();
            if (!submit_result) {
                return std::unexpected(submit_result.error());
            }

            io_uring_cqe* cqe;
            auto wait_result = ring_.wait_cqe(&cqe);
            if (!wait_result) {
                return std::unexpected(wait_result.error());
            }

            auto completion_result = process_completion(cqe);
            ring_.cqe_seen(cqe);
            if (!completion_result) {
                return std::unexpected(completion_result.error());
            }

            while (true) {
                auto peek_result = ring_.peek_cqe(&cqe);
                if (!peek_result) {
                    return std::unexpected(peek_result.error());
                }
                if (!peek_result.value()) {
                    break;
                }
                completion_result = process_completion(cqe);
                ring_.cqe_seen(cqe);
                if (!completion_result) {
                    return std::unexpected(completion_result.error());
                }
            }

            task_result = remove_completed_tasks();
            if (!task_result) {
                return std::unexpected(task_result.error());
            }
        }

        return {};
    }
    
    [[nodiscard]] OperationState* create_operation(
        std::coroutine_handle<> continuation) {
        assert_owner_thread();
        auto state = std::make_unique<OperationState>(continuation);
        auto* state_ptr = state.get();
        operations_.emplace(state_ptr, std::move(state));
        return state_ptr;
    }

    void mark_operation_pending(OperationState* state) noexcept {
        assert_owner_thread();
        state->status = OperationState::Status::Pending;
    }

    void discard_operation(OperationState* state) noexcept {
        assert_owner_thread();
        operations_.erase(state);
    }

    [[nodiscard]] std::expected<int, Error> take_operation_result(
        OperationState* state) {
        assert_owner_thread();
        auto operation = operations_.find(state);
        if (operation == operations_.end()) {
            return std::unexpected(Error(
                Error::Code::InvalidArgument,
                "Unknown operation result"));
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

#ifdef IO_URING_ASYNC_TESTING
    void test_fail_next_submit(Error error) {
        next_submit_error_ = std::move(error);
    }

    [[nodiscard]] std::expected<void, Error> test_process_completion(
        io_uring_cqe* cqe) {
        return process_completion(cqe);
    }

    [[nodiscard]] CancellationState* test_create_cancellation(
        OperationState* target) {
        auto cancellation = std::make_unique<CancellationState>(target);
        auto* cancellation_ptr = cancellation.get();
        target->cancellation_requested = true;
        cancellations_.emplace(cancellation_ptr, std::move(cancellation));
        return cancellation_ptr;
    }
#endif
    
    // The factory provides stable heap ownership. Executor-bound objects keep
    // raw references to this address, so the executor itself cannot be moved.
    IoUringExecutor(const IoUringExecutor&) = delete;
    IoUringExecutor& operator=(const IoUringExecutor&) = delete;
    IoUringExecutor(IoUringExecutor&&) = delete;
    IoUringExecutor& operator=(IoUringExecutor&&) = delete;
    
private:
    /**
     * @brief Private constructor (use create() factory)
     */
    explicit IoUringExecutor(IoUring&& ring, int wake_fd)
        : ring_(std::move(ring)), owner_thread_(std::this_thread::get_id()),
          wake_fd_(wake_fd) {}

    void assert_owner_thread() const noexcept {
        assert(owner_thread_ == std::this_thread::get_id() &&
               "IoUringExecutor used from a non-owner thread");
    }

    [[nodiscard]] std::expected<int, Error> submit_pending() {
#ifdef IO_URING_ASYNC_TESTING
        if (next_submit_error_) {
            auto error = std::move(*next_submit_error_);
            next_submit_error_.reset();
            return std::unexpected(std::move(error));
        }
#endif
        return ring_.submit();
    }

    [[nodiscard]] std::expected<void, Error> prepare_cancellations() {
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

    [[nodiscard]] bool has_user_operations() const noexcept {
        return operations_.size() > (wake_operation_ ? 1U : 0U);
    }

    [[nodiscard]] std::expected<void, Error> arm_wake() {
        if (wake_operation_ || stop_requested_.load(std::memory_order_acquire)) {
            return {};
        }

        auto* state = create_operation(std::noop_coroutine());
        state->executor_wake = true;
        auto buffer = std::as_writable_bytes(std::span{&wake_value_, 1U});
        auto preparation = ring_.prep_read(wake_fd_, buffer, 0, state);
        if (!preparation &&
            preparation.error().code() == Error::Code::QueueFull) {
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

    void fail_all_operations(const Error& error) {
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
    
    /**
     * @brief Process a completion event
     */
    [[nodiscard]] std::expected<void, Error> process_completion(
        io_uring_cqe* cqe) {
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

    [[nodiscard]] std::expected<void, Error> remove_completed_tasks() {
        for (auto task = active_tasks_.begin(); task != active_tasks_.end();) {
            if (!task->done()) {
                ++task;
                continue;
            }

            try {
                task->rethrow_if_failed();
            } catch (const std::exception& exception) {
                task = active_tasks_.erase(task);
                return std::unexpected(Error(Error::Code::TaskFailed,
                                             exception.what()));
            } catch (...) {
                task = active_tasks_.erase(task);
                return std::unexpected(Error(Error::Code::TaskFailed,
                                             "Unknown task exception"));
            }
            task = active_tasks_.erase(task);
        }
        return {};
    }
    
    IoUring ring_;
    std::list<Task<void>> active_tasks_;
    std::queue<std::list<Task<void>>::iterator> ready_queue_;
    std::unordered_map<OperationState*, std::unique_ptr<OperationState>> operations_;
    std::unordered_map<CancellationState*, std::unique_ptr<CancellationState>>
        cancellations_;
    std::thread::id owner_thread_;
    bool stopping_ = false;
    std::atomic_bool stop_requested_ = false;
    int wake_fd_ = -1;
    std::uint64_t wake_value_ = 0;
    OperationState* wake_operation_ = nullptr;
#ifdef IO_URING_ASYNC_TESTING
    std::optional<Error> next_submit_error_;
#endif
};

} // namespace io_uring_async_cpp23
