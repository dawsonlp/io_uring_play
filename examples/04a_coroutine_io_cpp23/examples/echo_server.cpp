/**
 * @file echo_server.cpp
 * @brief Simple TCP echo server using io_uring_async_cpp23
 * 
 * Demonstrates the primary TCP API:
 * - IoUringExecutor::create()  - Create the event loop
 * - TcpListener::bind()        - Bind and listen on a port
 * - TcpListener::accept()      - Accept incoming connections (co_await)
 * - TcpStream::receive()       - Receive data (co_await)
 * - TcpStream::send()          - Send data (co_await)
 * - Task<T>                    - Coroutine return type
 * - Error / std::expected      - C++23 error handling
 * 
 * Usage:
 *   ./echo_server [port]
 *   # In another terminal: echo "hello" | nc localhost 8080
 */

#include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>
#include "port_parser.hpp"

#include <print>
#include <array>
#include <optional>

using namespace io_uring_async_cpp23;

/**
 * @brief Handle a single client connection - echoes back everything received
 */
Task<void> handle_client(TcpStream client) {
    std::array<std::byte, 4096> buffer;
    
    while (true) {
        auto result = co_await client.receive(buffer);
        if (!result) {
            std::print(stderr, "Receive error: {}\n", 
                result.error().description());
            break;
        }
        
        if (result.value() == 0) {
            break;  // Client closed connection
        }
        
        auto send_result = co_await client.send(
            std::span{buffer.data(), result.value()});
        if (!send_result) {
            std::print(stderr, "Send error: {}\n", 
                send_result.error().description());
            break;
        }
    }
}

/**
 * @brief Main server coroutine - accepts connections and spawns handlers
 */
Task<void> server(IoUringExecutor& executor, uint16_t port,
                  std::optional<Error>& startup_error) {
    auto listener = TcpListener::bind(executor, port);
    if (!listener) {
        startup_error = listener.error();
        std::print(stderr, "Failed to bind: {}\n", 
            listener.error().description());
        co_return;
    }
    
    std::print("Echo server listening on port {}\n", listener->port());
    std::print("  Test with: echo 'hello' | nc localhost {}\n", 
        listener->port());
    
    while (true) {
        auto client_fd = co_await listener->accept();
        if (!client_fd) {
            std::print(stderr, "Accept error: {}\n", 
                client_fd.error().description());
            continue;
        }
        
        std::print("New connection: fd={}\n", client_fd.value());
        
        // Create a TcpStream from the accepted fd and handle it
        TcpStream stream(executor, client_fd.value());
        // This educational example handles one client at a time.
        co_await handle_client(std::move(stream));
    }
}

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::print(stderr, "Usage: {} [port]\n", argv[0]);
        return 2;
    }

    uint16_t port = 8080;
    if (argc == 2) {
        const auto parsed_port = example::parse_port(argv[1]);
        if (!parsed_port) {
            std::print(stderr, "Invalid port: {}\n", argv[1]);
            return 2;
        }
        port = *parsed_port;
    }

    std::print("io_uring_async_cpp23 v{}.{}.{} (C++{})\n",
        VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, CPP_STANDARD);
    
    auto executor = IoUringExecutor::create();
    if (!executor) {
        std::print(stderr, "Failed to create executor: {}\n",
            executor.error().description());
        return 1;
    }
    auto& runtime = **executor;
    
    std::optional<Error> startup_error;
    runtime.spawn(server(runtime, port, startup_error));
    
    auto result = runtime.run();
    if (!result) {
        std::print(stderr, "Executor error: {}\n",
            result.error().description());
        return 1;
    }
    if (startup_error) {
        return 1;
    }
    
    return 0;
}
