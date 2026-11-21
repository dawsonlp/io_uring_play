#include <stdexec/execution.hpp>

#include <liburing.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <memory>
#include <span>
#include <system_error>

namespace ex = stdexec;

namespace {

void require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

// One request at a time on the thread that drives the ring. This smoke test
// aborts on ring submission or wait failure because it cannot recover the ring.
class Ring {
public:
    Ring() {
        const int result = io_uring_queue_init(2, &ring_, 0);
        if (result < 0) {
            throw std::system_error(-result, std::generic_category(), "io_uring_queue_init");
        }
    }

    Ring(const Ring&) = delete;
    Ring& operator=(const Ring&) = delete;
    ~Ring() { io_uring_queue_exit(&ring_); }

    void read(int fd, std::span<char> buffer, off_t offset, void* operation,
              void (*complete)(void*, int) noexcept) noexcept {
        require(!complete_);
        auto* sqe = io_uring_get_sqe(&ring_);
        require(sqe);
        io_uring_prep_read(sqe, fd, buffer.data(), buffer.size(), offset);
        io_uring_sqe_set_data(sqe, operation);
        complete_ = complete;
        require(io_uring_submit(&ring_) == 1);
    }

    void run() {
        while (complete_) {
            io_uring_cqe* cqe = nullptr;
            int result;
            do {
                result = io_uring_wait_cqe(&ring_, &cqe);
            } while (result == -EINTR);
            require(result == 0);
            auto* operation = io_uring_cqe_get_data(cqe);
            const int bytes = cqe->res;
            auto complete = complete_;
            complete_ = nullptr;
            io_uring_cqe_seen(&ring_, cqe);
            complete(operation, bytes);
        }
    }

private:
    io_uring ring_{};
    void (*complete_)(void*, int) noexcept = nullptr;
};

struct Read {
    using sender_concept = ex::sender_t;
    using completion_signatures = ex::completion_signatures<
        ex::set_value_t(std::size_t), ex::set_error_t(std::error_code)>;

    Ring& ring;
    int fd;
    std::span<char> buffer;
    off_t offset;

    template<class Receiver>
    struct Operation {
        using operation_state_concept = ex::operation_state_t;
        Ring& ring;
        int fd;
        std::span<char> buffer;
        off_t offset;
        Receiver receiver;

        void start() & noexcept {
            ring.read(fd, buffer, offset, this, [](void* state, int result) noexcept {
                auto& op = *static_cast<Operation*>(state);
                if (result < 0) {
                    ex::set_error(std::move(op.receiver),
                                  std::error_code(-result, std::generic_category()));
                } else {
                    ex::set_value(std::move(op.receiver), static_cast<std::size_t>(result));
                }
            });
        }
    };

    template<class Receiver>
    auto connect(Receiver receiver) const {
        return Operation<Receiver>{ring, fd, buffer, offset, std::move(receiver)};
    }
};

struct ReadReceiver {
    using receiver_concept = ex::receiver_t;

    void* context;
    void (*on_value)(void*, std::size_t) noexcept;
    void (*on_error)(void*, std::error_code) noexcept;

    void set_value(std::size_t count) && noexcept { on_value(context, count); }
    void set_error(std::error_code code) && noexcept { on_error(context, code); }
};

using ReadOperation = decltype(ex::connect(std::declval<Read>(), std::declval<ReadReceiver>()));

struct ReadSequence {
    Ring& ring;
    int fd;
    std::array<char, 8> buffer{};
    std::unique_ptr<ReadOperation> operation;
    unsigned completions = 0;

    void start(int descriptor, off_t offset) {
        operation.reset(new ReadOperation(ex::connect(
            Read{ring, descriptor, buffer, offset},
            ReadReceiver{
                this,
                [](void* context, std::size_t count) noexcept {
                    auto& sequence = *static_cast<ReadSequence*>(context);
                    sequence.operation.reset();
                    switch (++sequence.completions) {
                    case 1:
                        require(count == 3);
                        require(sequence.buffer[0] == 'a'
                                && sequence.buffer[1] == 'b' && sequence.buffer[2] == 'c');
                        sequence.start(sequence.fd, 3);
                        return;
                    case 2:
                        require(count == 0);
                        sequence.start(-1, 0);
                        return;
                    default:
                        std::abort();
                    }
                },
                [](void* context, std::error_code error) noexcept {
                    auto& sequence = *static_cast<ReadSequence*>(context);
                    sequence.operation.reset();
                    require(++sequence.completions == 3);
                    require(error == std::errc::bad_file_descriptor);
                }
            })));
        ex::start(*operation);
    }
};

} // namespace

int main() {
    try {
        Ring ring;
        char path[] = "/tmp/io-uring-read-smoke.XXXXXX";
        const int fd = mkstemp(path);
        require(fd >= 0);
        require(unlink(path) == 0);
        require(write(fd, "abc", 3) == 3);
        ReadSequence sequence{ring, fd, {}, {}, 0};
        sequence.start(fd, 0);
        ring.run();
        require(sequence.completions == 3 && !sequence.operation);
        require(close(fd) == 0);
    } catch (const std::system_error& error) {
        if (error.code() == std::errc::operation_not_permitted ||
            error.code() == std::errc::function_not_supported) {
            return 77;
        }
        throw;
    }
}
