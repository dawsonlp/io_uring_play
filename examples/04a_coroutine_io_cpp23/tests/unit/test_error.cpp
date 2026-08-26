#include <io_uring_async_cpp23/core/error.hpp>

#include <cassert>
#include <cerrno>
#include <expected>

using namespace io_uring_async_cpp23;

int main() {
    Error error(Error::Code::IoError, "test error");
    assert(error.code() == Error::Code::IoError);
    assert(error.message() == "test error");
    assert(!error.is_success());

    const auto missing = Error::from_errno(ENOENT);
    assert(missing.code() == Error::Code::NotFound);
    assert(!missing.message().empty());

    const Error closed(Error::Code::ConnectionClosed, "peer reset");
    const auto description = closed.description();
    assert(description.find("Connection Closed") != std::string::npos);
    assert(description.find("peer reset") != std::string::npos);

    assert(Error(Error::Code::Timeout) == Error(Error::Code::Timeout, "detail"));
    assert(Error(Error::Code::Timeout) != Error(Error::Code::IoError));
    assert(Error::from_errno(EINTR).code() == Error::Code::Interrupted);

    const std::expected<int, Error> good = 42;
    const std::expected<int, Error> bad = std::unexpected(error);
    assert(good.value() == 42);
    assert(!bad.has_value());
    return 0;
}
