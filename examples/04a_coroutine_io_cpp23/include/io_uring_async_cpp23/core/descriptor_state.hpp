#pragma once

#include <memory>
#include <unistd.h>

namespace io_uring_async_cpp23 {

class DescriptorState {
public:
    explicit DescriptorState(int fd) noexcept : fd_(fd) {}

    ~DescriptorState() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    DescriptorState(const DescriptorState&) = delete;
    DescriptorState& operator=(const DescriptorState&) = delete;

    [[nodiscard]] int fd() const noexcept { return fd_; }

private:
    int fd_;
};

[[nodiscard]] inline std::shared_ptr<DescriptorState> adopt_descriptor(int fd) {
    try {
        return std::make_shared<DescriptorState>(fd);
    } catch (...) {
        ::close(fd);
        throw;
    }
}

} // namespace io_uring_async_cpp23
