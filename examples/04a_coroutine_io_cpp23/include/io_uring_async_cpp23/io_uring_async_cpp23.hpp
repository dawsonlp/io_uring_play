/**
 * @file io_uring_async_cpp23.hpp
 * @brief Main header file for the C++23 io_uring_async library
 * 
 * Include this single file to use the entire library.
 * 
 * Library API at a glance:
 * 
 * Core types:
 *   - Error              Error type with codes and descriptions
 *   - Task<T>            Coroutine return type for async computations
 *   - IoUring            RAII wrapper for the io_uring kernel interface
 * 
 * Runtime:
 *   - IoUringExecutor    Single-threaded event loop / task scheduler
 *   - ReadAwaitable      co_await a file read
 *   - WriteAwaitable     co_await a file write
 *   - AcceptAwaitable    co_await a TCP accept
 *   - SendAwaitable      co_await a TCP send
 *   - RecvAwaitable      co_await a TCP receive
 *   - CloseAwaitable     co_await an fd close
 * 
 * I/O:
 *   - TcpListener        Bind and accept TCP connections
 *   - TcpStream          Send/receive on a connected TCP socket
 * 
 * Quick start:
 * @code
 * #include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>
 * #include <print>
 * 
 * using namespace io_uring_async_cpp23;
 * 
 * Task<void> server(IoUringExecutor& executor) {
 *     auto listener = TcpListener::bind(executor, 8080);
 *     if (!listener) co_return;
 *     std::print("Listening on port 8080\n");
 *     // ... accept and handle connections
 * }
 * 
 * int main() {
 *     auto executor = IoUringExecutor::create();
 *     if (!executor) return 1;
 *     auto& runtime = **executor;
 *     runtime.spawn(server(runtime));
 *     runtime.run();
 * }
 * @endcode
 */

#pragma once

// Core types
#include "core/error.hpp"
#include "core/task.hpp"
#include "core/io_uring_wrapper.hpp"

// Runtime
#include "runtime/executor.hpp"
#include "runtime/awaitable.hpp"

// I/O Components
#include "io/tcp_listener.hpp"
#include "io/tcp_stream.hpp"

namespace io_uring_async_cpp23 {

/// Library version
inline constexpr int VERSION_MAJOR = 0;
inline constexpr int VERSION_MINOR = 2;
inline constexpr int VERSION_PATCH = 0;

/// C++ standard version this library targets
inline constexpr int CPP_STANDARD = 23;

} // namespace io_uring_async_cpp23
