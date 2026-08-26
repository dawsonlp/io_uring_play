#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <cassert>
#include <stdexcept>
#include <atomic>
#include <chrono>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

using namespace io_uring_async_cpp23;

Task<void> receive_until_cancelled(
    IoUringExecutor& executor,
    int fd,
    Error::Code& outcome) {
    TcpStream stream(executor, fd);
    std::array<std::byte, 32> buffer{};
    auto result = co_await stream.receive(buffer);
    outcome = result ? Error::Code::Success : result.error().code();
}

Task<Error::Code> nested_receive(TcpStream& stream) {
    std::array<std::byte, 8> buffer{};
    auto result = co_await stream.receive(buffer);
    co_return result ? Error::Code::Success : result.error().code();
}

Task<void> parent_awaiting_child(
    TcpStream& stream,
    Error::Code& child_outcome,
    bool& parent_finished) {
    child_outcome = co_await nested_receive(stream);
    parent_finished = true;
}

Task<void> accept_until_cancelled(
    TcpListener& listener,
    std::atomic_bool& started,
    Error::Code& outcome) {
    started.store(true, std::memory_order_release);
    auto result = co_await listener.accept();
    outcome = result ? Error::Code::Success : result.error().code();
    if (result) {
        ::close(result.value());
    }
}

void cancel_before_submission() {
    std::array<int, 2> sockets{};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);

    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;
    auto outcome = Error::Code::Unknown;
    runtime.spawn(receive_until_cancelled(runtime, sockets[0], outcome));

    assert(runtime.shutdown().has_value());
    assert(outcome == Error::Code::Cancelled);
    assert(!runtime.accepts_work());

    bool rejected = false;
    try {
        runtime.spawn(receive_until_cancelled(runtime, -1, outcome));
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);
    ::close(sockets[1]);
}

void cancel_after_submission() {
    std::array<int, 2> sockets{};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);

    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;
    auto outcome = Error::Code::Unknown;
    runtime.spawn(receive_until_cancelled(runtime, sockets[0], outcome));

    auto has_work = runtime.run_once();
    assert(has_work.has_value() && has_work.value());
    assert(runtime.shutdown().has_value());
    assert(outcome == Error::Code::Cancelled);
    ::close(sockets[1]);
}

void completed_operation_survives_shutdown() {
    std::array<int, 2> sockets{};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);

    const std::byte value{0x2a};
    assert(::send(sockets[1], &value, sizeof(value), 0) == 1);

    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;
    auto outcome = Error::Code::Unknown;
    runtime.spawn(receive_until_cancelled(runtime, sockets[0], outcome));

    auto has_work = runtime.run_once();
    assert(has_work.has_value());
    assert(runtime.shutdown().has_value());
    assert(outcome == Error::Code::Success);
    ::close(sockets[1]);
}

void nested_task_chain_unwinds_during_shutdown() {
    std::array<int, 2> sockets{};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);

    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;
    TcpStream stream(runtime, sockets[0]);
    auto child_outcome = Error::Code::Unknown;
    bool parent_finished = false;
    runtime.spawn(parent_awaiting_child(
        stream, child_outcome, parent_finished));

    auto has_work = runtime.run_once();
    assert(has_work.has_value() && has_work.value());
    assert(runtime.shutdown().has_value());
    assert(child_outcome == Error::Code::Cancelled);
    assert(parent_finished);
    ::close(sockets[1]);
}

void stop_request_wakes_blocked_run() {
    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    auto& runtime = **executor;
    auto listener_result = TcpListener::bind(runtime, 0);
    assert(listener_result.has_value());
    auto listener = std::move(listener_result.value());
    std::atomic_bool started = false;
    auto outcome = Error::Code::Unknown;
    runtime.spawn(accept_until_cancelled(listener, started, outcome));

    std::thread stopper([&] {
        while (!started.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        runtime.request_stop();
    });

    assert(runtime.run().has_value());
    stopper.join();
    assert(outcome == Error::Code::Cancelled);
}

int main() {
    cancel_before_submission();
    cancel_after_submission();
    completed_operation_survives_shutdown();
    nested_task_chain_unwinds_during_shutdown();
    stop_request_wakes_blocked_run();
    return 0;
}
