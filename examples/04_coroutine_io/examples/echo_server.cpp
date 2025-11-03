/**
 * @file echo_server.cpp
 * @brief Simple echo server example using io_uring_async
 * 
 * Demonstrates the library by implementing a TCP echo server that
 * accepts connections and echoes back any received data.
 */

#include "../include/io_uring_async/io_uring_async.hpp"
#include <iostream>
#include <csignal>
#include <atomic>

using namespace io_uring_async;

// Global flag for graceful shutdown
std::atomic<bool> should_stop{false};

void signal_handler(int) {
    should_stop.store(true);
}

/**
 * @brief Handle a single client connection
 * 
 * Echoes back all received data until the connection is closed.
 */
Task<void> handle_client(TcpStream client, int client_num) {
    std::cout << "Client " << client_num << " connected\n";
    
    std::array<std::byte, 4096> buffer;
    size_t total_bytes = 0;
    
    while (!should_stop.load()) {
        // Receive data from client
        auto recv_result = co_await client.receive(buffer);
        
        if (!recv_result) {
            std::cerr << "Client " << client_num << " recv error: " 
                      << recv_result.error().what() << "\n";
            break;
        }
        
        size_t n = recv_result.value();
        if (n == 0) {
            // Client closed connection
            std::cout << "Client " << client_num << " disconnected gracefully\n";
            break;
        }
        
        total_bytes += n;
        
        // Echo data back to client
        auto send_result = co_await client.send(std::span{buffer.data(), n});
        
        if (!send_result) {
            std::cerr << "Client " << client_num << " send error: " 
                      << send_result.error().what() << "\n";
            break;
        }
    }
    
    std::cout << "Client " << client_num << " connection closed "
              << "(echoed " << total_bytes << " bytes)\n";
    
    co_return;
}

/**
 * @brief Run the echo server
 * 
 * Accepts connections in a loop and spawns a handler for each client.
 */
Task<void> run_server(IoUringExecutor& executor, uint16_t port) {
    // Bind to port
    auto listener_result = TcpListener::bind(executor, port);
    
    if (!listener_result) {
        std::cerr << "Failed to bind to port " << port << ": " 
                  << listener_result.error().what() << "\n";
        co_return;
    }
    
    auto listener = std::move(listener_result.value());
    std::cout << "Echo server listening on port " << listener.port() << "\n";
    std::cout << "Press Ctrl+C to stop\n";
    
    int client_num = 0;
    
    while (!should_stop.load()) {
        // Accept a new connection
        auto accept_result = co_await listener.accept();
        
        if (!accept_result) {
            if (should_stop.load()) {
                break;  // Shutting down
            }
            std::cerr << "Accept error: " << accept_result.error().what() << "\n";
            continue;
        }
        
        // Create TcpStream from accepted file descriptor
        int client_fd = accept_result.value();
        TcpStream client{executor, client_fd};
        
        // Spawn a task to handle this client
        // Note: In a real implementation, we'd need to track these tasks
        // For now, they'll run independently
        executor.spawn(handle_client(std::move(client), ++client_num));
    }
    
    std::cout << "Server shutting down\n";
    co_return;
}

int main(int argc, char* argv[]) {
    // Parse port from command line
    uint16_t port = 8080;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    
    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << "io_uring_async echo server v"
              << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << "\n\n";
    
    // Create executor
    auto executor_result = IoUringExecutor::create(256);
    if (!executor_result) {
        std::cerr << "Failed to create executor: " 
                  << executor_result.error().what() << "\n";
        return 1;
    }
    
    auto executor = std::move(executor_result.value());
    
    // Spawn the server task
    executor.spawn(run_server(executor, port));
    
    // Run the event loop
    auto run_result = executor.run();
    if (!run_result) {
        std::cerr << "Executor error: " << run_result.error().what() << "\n";
        return 1;
    }
    
    return 0;
}
