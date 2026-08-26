#include <io_uring_async_cpp23/core/io_uring_wrapper.hpp>

#include <cassert>

using namespace io_uring_async_cpp23;

int main() {
    auto ring = IoUring::create(64);
    assert(ring.has_value());

    auto sqe = ring->get_sqe();
    assert(sqe.has_value());
    assert(sqe.value() != nullptr);
    return 0;
}
