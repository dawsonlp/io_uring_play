#pragma once

#include <cassert>
#include <cstdio>
#include <cstdlib>

namespace io_uring_async_cpp23::test {

[[noreturn]] inline void fail_check(
    const char* expression, const char* file, int line) noexcept {
    std::fprintf(stderr, "FAIL: %s:%d: check `%s` failed\n",
                 file, line, expression);
    std::fflush(stderr);
    std::exit(EXIT_FAILURE);
}

} // namespace io_uring_async_cpp23::test

// Tests use the familiar assertion spelling, but this check is always active,
// including optimized builds, and reports a stable non-signal failure status.
#undef assert
#define assert(expression)                                                     \
    ((expression) ? static_cast<void>(0)                                      \
                  : ::io_uring_async_cpp23::test::fail_check(                 \
                        #expression, __FILE__, __LINE__))
