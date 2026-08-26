#pragma once

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace io_uring_async_cpp23::example {

[[nodiscard]] inline std::optional<std::uint16_t> parse_port(
    std::string_view text) noexcept {
    if (text.empty()) {
        return std::nullopt;
    }

    unsigned value = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (error != std::errc{} || end != text.data() + text.size() ||
        value == 0 || value > 65535) {
        return std::nullopt;
    }

    return static_cast<std::uint16_t>(value);
}

} // namespace io_uring_async_cpp23::example
