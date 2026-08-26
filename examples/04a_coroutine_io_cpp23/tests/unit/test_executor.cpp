#include <io_uring_async_cpp23/runtime/executor.hpp>

#include <cassert>

using namespace io_uring_async_cpp23;

Task<void> set_value(int& value) {
    value = 42;
    co_return;
}

int main() {
    auto executor = IoUringExecutor::create(64);
    assert(executor.has_value());
    assert(executor->run().has_value());

    int value = 0;
    executor->spawn(set_value(value));
    assert(executor->run().has_value());
    assert(value == 42);
    return 0;
}
