#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <expected>

#include <unistd.h>

using namespace io_uring_async_cpp23;

struct FileResults {
    std::expected<std::size_t, Error> written;
    std::expected<std::size_t, Error> read;
    std::expected<std::size_t, Error> eof;
    std::array<std::byte, 8> buffer{};
};

Task<void> exercise_file_io(IoUringExecutor& executor, int fd,
                            FileResults& results) {
    constexpr std::array payload{
        std::byte{'i'}, std::byte{'o'}, std::byte{'_'}, std::byte{'u'},
        std::byte{'r'}, std::byte{'i'}, std::byte{'n'}, std::byte{'g'}};
    results.written = co_await WriteAwaitable(executor, fd, payload, 3);
    results.read = co_await ReadAwaitable(executor, fd, results.buffer, 3);

    std::array<std::byte, 1> beyond_end{};
    results.eof = co_await ReadAwaitable(executor, fd, beyond_end, 1024);
}

int main() {
    char path[] = "/tmp/io_uring_cpp23_file_test_XXXXXX";
    const int fd = ::mkstemp(path);
    assert(fd >= 0);
    assert(::unlink(path) == 0);

    auto executor = IoUringExecutor::create(8);
    assert(executor.has_value());
    FileResults results;
    executor.value()->spawn(exercise_file_io(**executor, fd, results));
    assert(executor.value()->run().has_value());

    assert(results.written.has_value() && *results.written == 8);
    assert(results.read.has_value() && *results.read == 8);
    constexpr std::array expected{
        std::byte{'i'}, std::byte{'o'}, std::byte{'_'}, std::byte{'u'},
        std::byte{'r'}, std::byte{'i'}, std::byte{'n'}, std::byte{'g'}};
    assert(results.buffer == expected);
    assert(results.eof.has_value() && *results.eof == 0);
    assert(::close(fd) == 0);
    return 0;
}
