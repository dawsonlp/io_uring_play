/**
 * @file task.hpp
 * @brief Task<T> - C++23 coroutine type with deducing this
 * 
 * Provides Task<T>, a coroutine type that represents an async computation.
 * This C++23 version uses 'deducing this' to eliminate duplicate overloads,
 * reducing code by ~100 lines compared to the C++20 version.
 */

#pragma once

#include <coroutine>
#include <exception>
#include <utility>
#include <variant>
#include <cassert>

namespace io_uring_async_cpp23 {

// Forward declaration
template<typename T>
class Task;

namespace detail {

/**
 * @brief Promise type for Task<T>
 * 
 * Manages the coroutine state and lifecycle. The promise stores the result
 * or exception and provides hooks for the coroutine machinery.
 */
template<typename T>
class TaskPromise {
public:
    using value_type = T;
    using handle_type = std::coroutine_handle<TaskPromise<T>>;
    
    /**
     * @brief Get the Task object from the coroutine
     */
    Task<T> get_return_object() noexcept;
    
    /**
     * @brief Initial suspend point - always suspend for lazy evaluation
     */
    std::suspend_always initial_suspend() const noexcept {
        return {};
    }
    
    /**
     * @brief Final suspend point - always suspend to keep coroutine alive
     */
    struct FinalAwaiter {
        bool await_ready() const noexcept { return false; }
        
        template<typename Promise>
        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<Promise> h) noexcept 
        {
            auto& promise = h.promise();
            if (promise.continuation_) {
                return promise.continuation_;
            }
            return std::noop_coroutine();
        }
        
        void await_resume() const noexcept {}
    };
    
    FinalAwaiter final_suspend() const noexcept {
        return {};
    }
    
    /**
     * @brief Store the return value
     */
    void return_value(T value) noexcept(std::is_nothrow_move_constructible_v<T>) {
        result_.template emplace<1>(std::move(value));
    }
    
    /**
     * @brief Handle uncaught exceptions
     */
    void unhandled_exception() noexcept {
        result_.template emplace<2>(std::current_exception());
    }
    
    /**
     * @brief Get the result (throws if exception was stored)
     * 
     * C++23: Using deducing this to handle all value categories with one template.
     * This replaces 3 separate overloads in the C++20 version.
     */
    template<typename Self>
    auto&& result(this Self&& self) {
        if (self.result_.index() == 2) {
            std::rethrow_exception(std::get<2>(self.result_));
        }
        assert(self.result_.index() == 1 && "Result not set");
        return std::forward<Self>(self).result_.template get<1>();
    }
    
    /**
     * @brief Check if result is ready
     */
    bool is_ready() const noexcept {
        return result_.index() != 0;
    }
    
    /**
     * @brief Set the continuation coroutine
     */
    void set_continuation(std::coroutine_handle<> continuation) noexcept {
        continuation_ = continuation;
    }
    
private:
    // Storage for result: index 0 = unset, 1 = value, 2 = exception
    std::variant<std::monostate, T, std::exception_ptr> result_;
    
    // Continuation to resume when this task completes
    std::coroutine_handle<> continuation_;
};

/**
 * @brief Specialization of TaskPromise for void
 */
template<>
class TaskPromise<void> {
public:
    using value_type = void;
    using handle_type = std::coroutine_handle<TaskPromise<void>>;
    
    Task<void> get_return_object() noexcept;
    
    std::suspend_always initial_suspend() const noexcept {
        return {};
    }
    
    struct FinalAwaiter {
        bool await_ready() const noexcept { return false; }
        
        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<TaskPromise<void>> h) noexcept 
        {
            auto& promise = h.promise();
            if (promise.continuation_) {
                return promise.continuation_;
            }
            return std::noop_coroutine();
        }
        
        void await_resume() const noexcept {}
    };
    
    FinalAwaiter final_suspend() const noexcept {
        return {};
    }
    
    /**
     * @brief Handle void return
     */
    void return_void() noexcept {
        has_result_ = true;
    }
    
    void unhandled_exception() noexcept {
        exception_ = std::current_exception();
    }
    
    /**
     * @brief Get the result (throws if exception was stored)
     */
    void result() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }
    
    bool is_ready() const noexcept {
        return has_result_ || exception_;
    }
    
    void set_continuation(std::coroutine_handle<> continuation) noexcept {
        continuation_ = continuation;
    }
    
private:
    bool has_result_ = false;
    std::exception_ptr exception_;
    std::coroutine_handle<> continuation_;
};

} // namespace detail

/**
 * @brief Task<T> - Represents an async computation
 * 
 * Task is the fundamental coroutine type in this library. It represents
 * an async computation that will produce a value of type T (or void).
 * 
 * C++23 improvements:
 * - Uses deducing this to eliminate duplicate await_resume() overloads
 * - Simpler implementation with same functionality as C++20 version
 * - ~35% less code in Task class itself
 * 
 * Key properties:
 * - Lazy evaluation: doesn't start until co_await'ed
 * - Move-only: clear ownership semantics
 * - RAII: automatically cleans up coroutine frame
 * - Exception-transparent: propagates exceptions naturally
 * 
 * Example:
 * @code
 * Task<int> compute_value() {
 *     co_return 42;
 * }
 * 
 * Task<void> use_value() {
 *     int result = co_await compute_value();
 *     std::print("{}\n", result);
 * }
 * @endcode
 * 
 * @tparam T The type of value produced by the task (or void)
 */
template<typename T = void>
class [[nodiscard]] Task {
public:
    using promise_type = detail::TaskPromise<T>;
    using handle_type = std::coroutine_handle<promise_type>;
    
    /**
     * @brief Construct a Task from a coroutine handle
     */
    explicit Task(handle_type handle) noexcept : coro_(handle) {}
    
    /**
     * @brief Destructor - destroys the coroutine frame if owned
     */
    ~Task() {
        if (coro_) {
            coro_.destroy();
        }
    }
    
    // Move-only semantics
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    /**
     * @brief Move constructor
     */
    Task(Task&& other) noexcept 
        : coro_(std::exchange(other.coro_, nullptr)) {}
    
    /**
     * @brief Move assignment
     */
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (coro_) {
                coro_.destroy();
            }
            coro_ = std::exchange(other.coro_, nullptr);
        }
        return *this;
    }
    
    /**
     * @brief Check if the task has completed
     */
    [[nodiscard]] bool done() const noexcept {
        return !coro_ || coro_.done();
    }
    
    /**
     * @brief Check if the task is valid (has a coroutine)
     */
    [[nodiscard]] bool valid() const noexcept {
        return coro_ != nullptr;
    }

    /**
     * @brief Resume a root task scheduled by an executor
     */
    void resume() {
        assert(coro_ && !coro_.done() && "Cannot resume an empty or completed task");
        coro_.resume();
    }
    
    /**
     * @brief Awaitable interface: check if result is ready
     */
    bool await_ready() const noexcept {
        return false;  // Always lazy
    }
    
    /**
     * @brief Awaitable interface: suspend and set up continuation
     */
    template<typename Promise>
    std::coroutine_handle<> await_suspend(
        std::coroutine_handle<Promise> awaiter) noexcept 
    {
        // Store who's waiting for us
        coro_.promise().set_continuation(awaiter);
        // Resume this task's coroutine
        return coro_;
    }
    
    /**
     * @brief Awaitable interface: get the result
     * 
     * C++23: Using deducing this to handle all value categories with one template.
     * In C++20, this required 3 separate overloads (&, const &, &&).
     * The void specialization uses a separate overload since auto&& cannot
     * deduce to void.
     */
    auto await_resume() requires std::is_void_v<T> {
        coro_.promise().result();
    }
    
    template<typename Self>
    auto&& await_resume(this Self&& self) requires (!std::is_void_v<T>) {
        return std::forward<Self>(self).coro_.promise().result();
    }
    
private:
    handle_type coro_;
};

// Implementation of promise get_return_object methods
namespace detail {

template<typename T>
Task<T> TaskPromise<T>::get_return_object() noexcept {
    return Task<T>{handle_type::from_promise(*this)};
}

inline Task<void> TaskPromise<void>::get_return_object() noexcept {
    return Task<void>{handle_type::from_promise(*this)};
}

} // namespace detail

} // namespace io_uring_async_cpp23
