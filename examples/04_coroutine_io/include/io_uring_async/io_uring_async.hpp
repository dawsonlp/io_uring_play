/**
 * @file io_uring_async.hpp
 * @brief Main header file for io_uring_async library
 * 
 * Include this file to use the library.
 */

#pragma once

// Core types
#include "core/result.hpp"
#include "core/error.hpp"
#include "core/task.hpp"
#include "core/io_uring_wrapper.hpp"

// Runtime
#include "runtime/executor.hpp"
#include "runtime/awaitable.hpp"

// I/O Components
#include "io/tcp_listener.hpp"
#include "io/tcp_stream.hpp"

namespace io_uring_async {

// Version information
inline constexpr int VERSION_MAJOR = 0;
inline constexpr int VERSION_MINOR = 1;
inline constexpr int VERSION_PATCH = 0;

} // namespace io_uring_async
