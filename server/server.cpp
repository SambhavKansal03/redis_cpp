#include "server/server.h"
#include "server/client_handler.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <cstring>

Server::Server(int port, KVStore& store, size_t num_threads)
    : port_(port), server_fd_(-1), store_(store), pool_(num_threads)
{
    setup_socket();
}

void Server::setup_socket() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error("Failed to bind to port " + std::to_string(port_));
    }

    // Increased backlog from 5 → 128: with a thread pool we can handle
    // bursts of connections without dropping them at the OS level.
    if (listen(server_fd_, 128) < 0) {
        throw std::runtime_error("Failed to listen");
    }

    std::cout << "[server] listening on port " << port_ << "\n";
}

void Server::run() {
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd_,
                               (struct sockaddr*)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            std::cerr << "[server] accept failed\n";
            continue;
        }

        std::cout << "[server] client connected (fd=" << client_fd << ")\n";

        // Phase 3: instead of blocking here to handle the client,
        // we hand the client_fd off to the thread pool.
        //
        // The lambda captures client_fd by value (copies the int) and
        // store_ by reference (safe — store_ outlives all threads).
        //
        // The accept loop immediately returns to accept the next client —
        // the pool worker handles this client in the background.
        pool_.enqueue([client_fd, &store = store_] {
            handle_client_connection(client_fd, store);
            close(client_fd);
            std::cout << "[server] client disconnected (fd=" << client_fd << ")\n";
        });
    }
}