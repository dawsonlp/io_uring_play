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
#include <thread>

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
 * executor->spawn(my_async_task());
 * auto result = executor->run();
 * @endcode
 */
class IoUringExecutor {
public:
    /**
     * @brief Create an executor with the specified queue depth
     * 
     * @param queue_depth io_uring queue depth (default 256)
     * @return std::expected containing executor or error
     */
    [[nodiscard]] static std::expected<IoUringExecutor, Error> create(
        unsigned queue_depth = 256) noexcept 
    {
        auto ring_result = IoUring::create(queue_depth);
        if (!ring_result) {
            return std::unexpected(ring_result.error());
        }
        
        return IoUringExecutor(std::move(ring_result.value()));
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
        while (!ready_queue_.empty() || !operations_.empty()) {
            // Process all ready tasks
            while (!ready_queue_.empty()) {
                auto task = ready_queue_.front();
                ready_queue_.pop();
                task->resume();
            }

            remove_completed_tasks();
            
            // If we have pending I/O operations, wait for completions
            if (!operations_.empty()) {
                auto submit_result = ring_.submit();
                if (!submit_result) {
                    return std::unexpected(submit_result.error());
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

                remove_completed_tasks();
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
        // Process ready tasks
        size_t tasks_processed = 0;
        const size_t max_tasks = ready_queue_.size();
        
        while (!ready_queue_.empty() && tasks_processed < max_tasks) {
            auto task = ready_queue_.front();
            ready_queue_.pop();
            task->resume();
            tasks_processed++;
        }

        remove_completed_tasks();
        
        // Check for I/O completions
        if (!operations_.empty()) {
            auto submit_result = ring_.submit();
            if (!submit_result) {
                return std::unexpected(submit_result.error());
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

                remove_completed_tasks();
            }
        }
        
        bool has_work = !ready_queue_.empty() || !operations_.empty();
        return has_work;
    }
    
    /**
     * @brief Get the io_uring wrapper (for advanced usage)
     */
    [[nodiscard]] IoUring& ring() noexcept {
        assert_owner_thread();
        return ring_;
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
        if (operation == operations_.end() ||
            operation->second->status != OperationState::Status::Completed) {
            return std::unexpected(Error(
                Error::Code::InvalidArgument,
                "Operation result requested before completion"));
        }

        const int result = operation->second->result;
        operations_.erase(operation);
        return result;
    }
    
    // Move-only semantics
    IoUringExecutor(const IoUringExecutor&) = delete;
    IoUringExecutor& operator=(const IoUringExecutor&) = delete;
    IoUringExecutor(IoUringExecutor&&) = default;
    IoUringExecutor& operator=(IoUringExecutor&&) = default;
    
private:
    /**
     * @brief Private constructor (use create() factory)
     */
    explicit IoUringExecutor(IoUring&& ring)
        : ring_(std::move(ring)), owner_thread_(std::this_thread::get_id()) {}

    void assert_owner_thread() const noexcept {
        assert(owner_thread_ == std::this_thread::get_id() &&
               "IoUringExecutor used from a non-owner thread");
    }
    
    /**
     * @brief Process a completion event
     */
    [[nodiscard]] std::expected<void, Error> process_completion(
        io_uring_cqe* cqe) {
        auto* state = static_cast<OperationState*>(io_uring_cqe_get_data(cqe));
        auto operation = operations_.find(state);
        if (operation == operations_.end() ||
            operation->second->status != OperationState::Status::Pending) {
            return std::unexpected(Error(
                Error::Code::InvalidArgument,
                "Unknown, duplicate, or stale io_uring completion"));
        }

        state->result = cqe->res;
        state->status = OperationState::Status::Completed;
        auto continuation = state->continuation;
        if (continuation) {
            continuation.resume();
        }
        return {};
    }

    void remove_completed_tasks() {
        active_tasks_.remove_if([](const Task<void>& task) {
            return task.done();
        });
    }
    
    IoUring ring_;
    std::list<Task<void>> active_tasks_;
    std::queue<std::list<Task<void>>::iterator> ready_queue_;
    std::unordered_map<OperationState*, std::unique_ptr<OperationState>> operations_;
    std::thread::id owner_thread_;
};

} // namespace io_uring_async_cpp23
