#include <io_uring_async_cpp23/core/io_uring_wrapper.hpp>

#include <cassert>
#include <array>
#include <cstddef>

using namespace io_uring_async_cpp23;

int main() {
    auto ring = IoUring::create(64);
    assert(ring.has_value());

    std::array<std::byte, 1> buffer{};
    auto preparation = ring->prep_read(-1, buffer, 0, nullptr);
    assert(preparation.has_value());
    return 0;
}
