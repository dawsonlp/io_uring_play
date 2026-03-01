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
#include <expected>
#include <vector>
#include <queue>
#include <unordered_map>
#include <memory>
#include <functional>

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
     * @return std::expected<void, Error>
     */
    [[nodiscard]] std::expected<void, Error> run() {
        while (!ready_queue_.empty() || !pending_operations_.empty()) {
            // Process all ready tasks
            while (!ready_queue_.empty()) {
                auto task = std::move(ready_queue_.front());
                ready_queue_.pop();
                // The task's coroutine will be resumed when it is co_await'ed
                // or when a continuation resumes it.
            }
            
            // If we have pending I/O operations, wait for completions
            if (!pending_operations_.empty()) {
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
                
                process_completion(cqe);
                ring_.cqe_seen(cqe);
                
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
     * @return std::expected<bool, Error> - true if more work remains
     */
    [[nodiscard]] std::expected<bool, Error> run_once() {
        // Process ready tasks
        size_t tasks_processed = 0;
        const size_t max_tasks = ready_queue_.size();
        
        while (!ready_queue_.empty() && tasks_processed < max_tasks) {
            auto task = std::move(ready_queue_.front());
            ready_queue_.pop();
            tasks_processed++;
        }
        
        // Check for I/O completions
        if (!pending_operations_.empty()) {
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
                    process_completion(cqe);
                    ring_.cqe_seen(cqe);
                    
                    peek_result = ring_.peek_cqe(&cqe);
                    if (!peek_result) {
                        return std::unexpected(peek_result.error());
                    }
                } while (peek_result.value());
            }
        }
        
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
     * @param operation_id Unique ID for the operation
     * @param continuation The coroutine to resume when operation completes
     */
    void register_operation(uint64_t operation_id, 
                           std::coroutine_handle<> continuation) {
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
    
    /**
     * @brief Allocate a unique operation ID
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
        return -EINVAL;
    }
    
    /**
     * @brief Clear the result of a completed operation
     * 
     * @param operation_id The operation ID
     */
    void clear_operation_result(uint64_t operation_id) {
        operation_results_.erase(operation_id);
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
        : ring_(std::move(ring))
        , next_operation_id_(1) {}
    
    /**
     * @brief Process a completion event
     */
    void process_completion(io_uring_cqe* cqe) {
        uint64_t operation_id = reinterpret_cast<uint64_t>(
            io_uring_cqe_get_data(cqe));
        
        auto it = pending_operations_.find(operation_id);
        if (it == pending_operations_.end()) {
            return;  // Unknown operation
        }
        
        auto continuation = it->second;
        pending_operations_.erase(it);
        
        // Store result for the awaitable to retrieve
        operation_results_[operation_id] = cqe->res;
        
        // Resume the waiting coroutine
        if (continuation) {
            continuation.resume();
        }
    }
    
    IoUring ring_;
    std::queue<Task<void>> ready_queue_;
    std::unordered_map<uint64_t, std::coroutine_handle<>> pending_operations_;
    std::unordered_map<uint64_t, int> operation_results_;
    uint64_t next_operation_id_;
};

} // namespace io_uring_async_cpp23
