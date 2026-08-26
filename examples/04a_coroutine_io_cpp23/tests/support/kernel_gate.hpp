#pragma once

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <liburing.h>

namespace io_uring_async_cpp23::test {

inline constexpr int skip_exit_status = 77;

class KernelGate {
public:
    KernelGate() noexcept {
        if (std::getenv("IO_URING_TEST_FORCE_SKIP")) {
            std::fprintf(stderr,
                         "SKIP: io_uring unavailability forced by test policy\n");
            std::fflush(stderr);
            std::exit(skip_exit_status);
        }

        io_uring ring{};
        const int result = io_uring_queue_init(2, &ring, 0);
        if (result >= 0) {
            io_uring_queue_exit(&ring);
            return;
        }

        const int error = -result;
        if (error == EPERM || error == EACCES || error == ENOSYS ||
            error == EOPNOTSUPP) {
            std::fprintf(stderr,
                         "SKIP: io_uring unavailable on this host: %s (%d)\n",
                         std::strerror(error), error);
            std::fflush(stderr);
            std::exit(skip_exit_status);
        }
    }
};

inline const KernelGate kernel_gate;

} // namespace io_uring_async_cpp23::test
