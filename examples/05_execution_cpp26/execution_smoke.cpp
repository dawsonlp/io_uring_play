#include <stdexec/execution.hpp>

#include <cstdlib>
#include <functional>
#include <memory>
#include <thread>

namespace ex = stdexec;

namespace {

void check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

struct DestroyingReceiver {
    using receiver_concept = ex::receiver_t;
    std::function<void()> destroy;

    void set_value() && noexcept {
        auto destroy_state = std::move(destroy);
        destroy_state();
    }
};

void check_inline_completion() {
    using Operation = decltype(ex::connect(ex::just(), DestroyingReceiver{}));
    std::unique_ptr<Operation> operation;
    const auto caller = std::this_thread::get_id();
    operation.reset(new Operation(ex::connect(ex::just(), DestroyingReceiver{
        [&] {
            check(std::this_thread::get_id() == caller);
            operation.reset();
        }
    })));
    ex::start(*operation);
    check(!operation);
}

void check_composition() {
    auto result = ex::sync_wait(ex::when_all(
        ex::just(20) | ex::let_value([](int& value) { return ex::just(value * 2); }),
        ex::just(2)));
    check(result.has_value());
    const auto [first, second] = *result;
    check(first + second == 42);

    auto recovered = ex::sync_wait(ex::just_error(41)
        | ex::upon_error([](int error) { return error + 1; }));
    check(recovered && std::get<0>(*recovered) == 42);
    auto stopped = ex::sync_wait(ex::just_stopped() | ex::upon_stopped([] { return 42; }));
    check(stopped && std::get<0>(*stopped) == 42);
}

ex::task<int> coroutine_answer() {
    const int first = co_await ex::just(20);
    const int second = co_await ex::just(first + 2);
    co_return first + second;
}

void check_stop_token() {
    ex::inplace_stop_source source;
    source.request_stop();
    ex::run_loop loop;
    std::jthread worker([&] { loop.run(); });
    auto sender = ex::schedule(loop.get_scheduler())
        | ex::then([] { std::abort(); })
        | ex::upon_stopped([&] { loop.finish(); })
        | ex::write_env(ex::prop{ex::get_stop_token, source.get_token()});
    check(ex::sync_wait(std::move(sender)).has_value());
}

void check_scheduler_hop() {
    ex::run_loop loop;
    std::jthread worker([&] { loop.run(); });
    const auto worker_id = worker.get_id();
    const auto caller = std::this_thread::get_id();
    auto sender = ex::just() | ex::then([&] {
        check(std::this_thread::get_id() == caller);
    });
    auto result = ex::sync_wait(ex::continues_on(std::move(sender), loop.get_scheduler())
        | ex::then([&] {
            check(std::this_thread::get_id() == worker_id);
            loop.finish();
        }));
    check(result.has_value());
}

} // namespace

int main() {
    check_inline_completion();
    check_composition();
    check_scheduler_hop();
    check_stop_token();
    auto answer = ex::sync_wait(coroutine_answer());
    check(answer && std::get<0>(*answer) == 42);
}
