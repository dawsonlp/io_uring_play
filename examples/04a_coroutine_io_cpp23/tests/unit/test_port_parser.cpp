#include "../../examples/port_parser.hpp"

#include <cassert>
#include <cstdint>
#include <string_view>

using io_uring_async_cpp23::example::parse_port;

int main() {
    assert(parse_port("1") == std::uint16_t{1});
    assert(parse_port("8080") == std::uint16_t{8080});
    assert(parse_port("65535") == std::uint16_t{65535});
    assert(parse_port("00080") == std::uint16_t{80});

    for (const std::string_view invalid : {
             "", "0", "65536", "999999999999999999999999",
             "8080junk", " 8080", "+8080", "-1", "8 080", "http"}) {
        assert(!parse_port(invalid).has_value());
    }
    return 0;
}
