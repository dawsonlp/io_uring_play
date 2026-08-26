#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

using namespace io_uring_async_cpp23;

int main() {
    static_assert(CPP_STANDARD == 23);
    static_assert(!std::is_copy_constructible_v<IoUring>);
    static_assert(std::is_move_constructible_v<IoUring>);
    static_assert(!std::is_copy_constructible_v<IoUringExecutor>);
    static_assert(!std::is_move_constructible_v<IoUringExecutor>);
    static_assert(!std::is_copy_constructible_v<TcpStream>);
    static_assert(std::is_nothrow_move_constructible_v<TcpStream>);
    static_assert(!std::default_initializable<TcpStream>);
    static_assert(!std::constructible_from<TcpStream, int>);
    static_assert(!std::is_copy_constructible_v<TcpListener>);
    static_assert(std::is_nothrow_move_constructible_v<TcpListener>);
    static_assert(!std::default_initializable<TcpListener>);
    static_assert(!std::default_initializable<IoUringExecutor>);
    static_assert(!std::is_copy_constructible_v<Task<void>>);
    static_assert(std::is_nothrow_move_constructible_v<Task<void>>);
    static_assert(std::same_as<
        decltype(std::declval<TcpStream&>().send(
            std::declval<std::span<const std::byte>>())),
        SendAwaitable>);
    static_assert(std::same_as<
        decltype(std::declval<TcpStream&>().receive(
            std::declval<std::span<std::byte>>())),
        RecvAwaitable>);
    static_assert(std::same_as<
        decltype(std::declval<TcpListener&>().accept()), AcceptAwaitable>);
    static_assert(std::same_as<decltype(std::declval<TcpStream&>().close()),
                               void>);
    assert(VERSION_MAJOR == 0);
    return 0;
}
