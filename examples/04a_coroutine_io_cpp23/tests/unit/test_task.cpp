#include <io_uring_async_cpp23/core/task.hpp>

#include <cassert>
#include <stdexcept>
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

    bool empty_rejected = false;
    try {
        value_task.resume();
    } catch (const std::logic_error&) {
        empty_rejected = true;
    }
    assert(empty_rejected);

    void_task.resume();
    assert(void_task.done());
    bool completed_rejected = false;
    try {
        void_task.resume();
    } catch (const std::logic_error&) {
        completed_rejected = true;
    }
    assert(completed_rejected);
    return 0;
}
