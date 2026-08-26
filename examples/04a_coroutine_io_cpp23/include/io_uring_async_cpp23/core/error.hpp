/**
 * @file error.hpp
 * @brief Error type for the io_uring async library
 * 
 * Provides a comprehensive error type that can represent various I/O errors.
 * This file is identical to the C++20 version as error handling basics haven't changed.
 */

#pragma once

#include <string>
#include <system_error>
#include <cstring>

namespace io_uring_async_cpp23 {

/**
 * @brief Error type for I/O operations
 * 
 * Represents errors that can occur during I/O operations.
 * Can be constructed from errno values or custom error messages.
 */
class Error {
public:
    /**
     * @brief Error codes
     */
    enum class Code {
        Success = 0,
        IoError,
        ConnectionClosed,
        Timeout,
        InvalidArgument,
        OutOfMemory,
        QueueFull,
        PermissionDenied,
        NotFound,
        AlreadyExists,
        WouldBlock,
        Unknown
    };
    
    /**
     * @brief Construct an error from an error code
     */
    explicit Error(Code code, std::string message = "")
        : code_(code), message_(std::move(message)) {}
    
    /**
     * @brief Construct an error from errno
     */
    static Error from_errno(int err) {
        Code code = errno_to_code(err);
        return Error(code, std::strerror(err));
    }
    
    /**
     * @brief Get the error code
     */
    [[nodiscard]] Code code() const noexcept { return code_; }
    
    /**
     * @brief Get the error message
     */
    [[nodiscard]] const std::string& message() const noexcept { 
        return message_; 
    }
    
    /**
     * @brief Check if this represents success (no error)
     */
    [[nodiscard]] bool is_success() const noexcept { 
        return code_ == Code::Success; 
    }
    
    /**
     * @brief Get a human-readable description
     */
    [[nodiscard]] std::string description() const {
        std::string result = code_to_string(code_);
        if (!message_.empty()) {
            result += ": " + message_;
        }
        return result;
    }
    
    /**
     * @brief Equality comparison
     */
    [[nodiscard]] bool operator==(const Error& other) const noexcept {
        return code_ == other.code_;
    }
    
    [[nodiscard]] bool operator!=(const Error& other) const noexcept {
        return !(*this == other);
    }
    
private:
    /**
     * @brief Convert errno to error code
     */
    static Code errno_to_code(int err) noexcept {
        switch (err) {
            case 0: return Code::Success;
            case EACCES:
            case EPERM: return Code::PermissionDenied;
            case ENOENT: return Code::NotFound;
            case EEXIST: return Code::AlreadyExists;
            case EAGAIN: return Code::WouldBlock;
#if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK: return Code::WouldBlock;
#endif
            case ENOMEM: return Code::OutOfMemory;
            case EINVAL: return Code::InvalidArgument;
            case ETIMEDOUT: return Code::Timeout;
            case ECONNRESET:
            case EPIPE: return Code::ConnectionClosed;
            default: return Code::IoError;
        }
    }
    
    /**
     * @brief Convert error code to string
     */
    static std::string code_to_string(Code code) {
        switch (code) {
            case Code::Success: return "Success";
            case Code::IoError: return "I/O Error";
            case Code::ConnectionClosed: return "Connection Closed";
            case Code::Timeout: return "Timeout";
            case Code::InvalidArgument: return "Invalid Argument";
            case Code::OutOfMemory: return "Out of Memory";
            case Code::QueueFull: return "Submission Queue Full";
            case Code::PermissionDenied: return "Permission Denied";
            case Code::NotFound: return "Not Found";
            case Code::AlreadyExists: return "Already Exists";
            case Code::WouldBlock: return "Would Block";
            case Code::Unknown: return "Unknown Error";
        }
        return "Unknown Error";
    }
    
    Code code_;
    std::string message_;
};

} // namespace io_uring_async_cpp23
