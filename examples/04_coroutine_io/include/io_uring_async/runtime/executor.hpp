/**
 * @file executor.hpp
 * @brief IoUringExecutor - Event loop for async I/O operations
 * 
 * The executor is the runtime that manages the event loop, schedules coroutines,
 * submits operations to io_uring, and processes completions to resume coroutines.
 * 
 * This is a single-threaded executor designed for clarity and predictable performance.
 */

#pragma once

#include "../core/io_uring_wrapper.hpp"
#include "../core/task.hpp"
#include "../core/result.hpp"
#include "../core/error.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <memory>
#include <functional>

namespace io_uring_async {

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
 * IoUringExecutor executor;
 * executor.spawn(my_async_task());
 * executor.run();  // Blocks until all tasks complete
 * @endcode
 */
class IoUringExecutor {
public:
    /**
     * @brief Construct an executor
     * 
     * @param queue_depth io_uring queue depth (default 256)
     * @return Result containing executor or error
     */
    [[nodiscard]] static Result<IoUringExecutor, Error> create(
        unsigned queue_depth = 256) noexcept 
    {
        auto ring_result = IoUring::create(queue_depth);
        if (!ring_result) {
            return Unexpected(ring_result.error());
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
        // Start the task by resuming its coroutine handle
        // Tasks are lazy, so this initiates execution
        if (task.valid()) {
            ready_queue_.push(std::move(task));
        }
    }
    
    /**
     * @brief Run the event loop until all tasks complete
     * 
     * This processes ready tasks and waits for I/O completions
     * until there are no more pending operations.
     * 
     * @return Result<void, Error>
     */
    [[nodiscard]] Result<void, Error> run() {
        while (!ready_queue_.empty() || !pending_operations_.empty()) {
            // Process all ready tasks
            while (!ready_queue_.empty()) {
                auto task = std::move(ready_queue_.front());
                ready_queue_.pop();
                
                // Resume the task
                // If it co_awaits an I/O operation, it will be added to pending_operations_
                // If it completes, it will be destroyed
                // The task's await_suspend will handle the continuation
            }
            
            // If we have pending I/O operations, wait for completions
            if (!pending_operations_.empty()) {
                // Submit any pending operations
                auto submit_result = ring_.submit();
                if (!submit_result) {
                    return Unexpected(submit_result.error());
                }
                
                // Wait for at least one completion
                io_uring_cqe* cqe;
                auto wait_result = ring_.wait_cqe(&cqe);
                if (!wait_result) {
                    return Unexpected(wait_result.error());
                }
                
                // Process the completion
                process_completion(cqe);
                ring_.cqe_seen(cqe);
                
                // Check for more completions without blocking
                while (true) {
                    io_uring_cqe* peek_cqe;
                    auto peek_result = ring_.peek_cqe(&peek_cqe);
                    
                    if (!peek_result) {
                        return Unexpected(peek_result.error());
                    }
                    
                    if (!peek_result.value()) {
                        break;  // No more CQEs available
                    }
                    
                    process_completion(peek_cqe);
                    ring_.cqe_seen(peek_cqe);
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
     * @return Result<bool, Error> - true if more work remains, false if done
     */
    [[nodiscard]] Result<bool, Error> run_once() {
        // Process ready tasks
        size_t tasks_processed = 0;
        const size_t max_tasks = ready_queue_.size();
        
        while (!ready_queue_.empty() && tasks_processed < max_tasks) {
            auto task = std::move(ready_queue_.front());
            ready_queue_.pop();
            tasks_processed++;
            
            // Task will either complete or suspend waiting for I/O
        }
        
        // Check for I/O completions
        if (!pending_operations_.empty()) {
            // Submit pending operations
            auto submit_result = ring_.submit();
            if (!submit_result) {
                return Unexpected(submit_result.error());
            }
            
            // Check for completions (non-blocking)
            io_uring_cqe* cqe;
            auto peek_result = ring_.peek_cqe(&cqe);
            
            if (!peek_result) {
                return Unexpected(peek_result.error());
            }
            
            if (peek_result.value()) {
                // Process available completions
                do {
                    process_completion(cqe);
                    ring_.cqe_seen(cqe);
                    
                    peek_result = ring_.peek_cqe(&cqe);
                    if (!peek_result) {
                        return Unexpected(peek_result.error());
                    }
                } while (peek_result.value());
            }
        }
        
        // Return true if more work remains
        bool has_work = !ready_queue_.empty() || !pending_operations_.empty();
        return has_work;
    }
    
    /**
     * @brief Get the io_uring wrapper (for advanced usage)
     */
    [[nodiscard]] IoUring& ring() noexcept {
        return ring_;
    }
    
    /**
     * @brief Register a pending operation
     * 
     * Called by awaitables when they submit operations to io_uring.
     * 
     * @param operation_id Unique ID for the operation (from io_uring user_data)
     * @param continuation The coroutine to resume when operation completes
     */
    void register_operation(uint64_t operation_id, std::coroutine_handle<> continuation) {
        pending_operations_[operation_id] = continuation;
    }
    
    /**
     * @brief Unregister a pending operation
     * 
     * @param operation_id The operation ID to remove
     */
    void unregister_operation(uint64_t operation_id) {
        pending_operations_.erase(operation_id);
    }
    
private:
    /**
     * @brief Private constructor (use create() factory)
     */
    explicit IoUringExecutor(IoUring&& ring) 
        : ring_(std::move(ring))
        , next_operation_id_(1) {}
    
    /**
     * @brief Process a completion event
     * 
     * Extracts the operation ID, finds the waiting coroutine,
     * and resumes it with the result.
     */
    void process_completion(io_uring_cqe* cqe) {
        // Get the operation ID from user_data
        uint64_t operation_id = reinterpret_cast<uint64_t>(io_uring_cqe_get_data(cqe));
        
        // Find the waiting coroutine
        auto it = pending_operations_.find(operation_id);
        if (it == pending_operations_.end()) {
            // Operation not found - this shouldn't happen in normal operation
            return;
        }
        
        // Get the continuation and remove from pending
        auto continuation = it->second;
        pending_operations_.erase(it);
        
        // Store the result for the awaitable to retrieve
        // The awaitable's await_resume will read this
        operation_results_[operation_id] = cqe->res;
        
        // Resume the waiting coroutine
        if (continuation) {
            continuation.resume();
        }
    }
    
    IoUring ring_;                                          ///< The io_uring instance
    std::queue<Task<void>> ready_queue_;                   ///< Tasks ready to run
    std::unordered_map<uint64_t, std::coroutine_handle<>> pending_operations_;  ///< Waiting operations
    std::unordered_map<uint64_t, int> operation_results_;  ///< Operation results
    uint64_t next_operation_id_;                           ///< Next operation ID to assign
    
public:
    /**
     * @brief Allocate a unique operation ID
     * 
     * Used by awaitables to identify their operations.
     */
    [[nodiscard]] uint64_t allocate_operation_id() noexcept {
        return next_operation_id_++;
    }
    
    /**
     * @brief Get the result of a completed operation
     * 
     * @param operation_id The operation ID
     * @return The operation result (bytes transferred or negative errno)
     */
    [[nodiscard]] int get_operation_result(uint64_t operation_id) const {
        auto it = operation_results_.find(operation_id);
        if (it != operation_results_.end()) {
            return it->second;
        }
        return -EINVAL;  // Operation not found
    }
    
    /**
     * @brief Clear the result of an operation
     * 
     * @param operation_id The operation ID
     */
    void clear_operation_result(uint64_t operation_id) {
        operation_results_.erase(operation_id);
    }
};

} // namespace io_uring_async
