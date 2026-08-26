#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <cassert>

using namespace io_uring_async_cpp23;

int main() {
    static_assert(CPP_STANDARD == 23);
    assert(VERSION_MAJOR == 0);
    return 0;
}
