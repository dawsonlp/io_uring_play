#include <io_uring_async_cpp23/core/task.hpp>

#include <cassert>
#include <utility>

using namespace io_uring_async_cpp23;

Task<int> compute_value() {
    co_return 42;
}

Task<void> do_nothing() {
    co_return;
}

int main() {
    auto value_task = compute_value();
    auto void_task = do_nothing();
    assert(value_task.valid());
    assert(void_task.valid());

    auto moved = std::move(value_task);
    assert(moved.valid());
    assert(!value_task.valid());
    return 0;
}
