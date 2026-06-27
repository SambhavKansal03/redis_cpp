#include "server/server.h"
#include "server/client_handler.h"
#include "server/logger.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>

Server::Server(int port, KVStore& store, size_t num_threads)
    : port_(port), server_fd_(-1), store_(store), pool_(num_threads)
{
    setup_socket();
    LOG("[server] thread pool size: " << num_threads);
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

    // Backlog 128: handles connection bursts without dropping at OS level
    if (listen(server_fd_, 128) < 0) {
        throw std::runtime_error("Failed to listen");
    }

    LOG("[server] listening on port " << port_);
}

void Server::run() {
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd_,
                               (struct sockaddr*)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            LOG("[server] accept failed");
            continue;
        }

        LOG("[server] client connected (fd=" << client_fd << ")");

        pool_.enqueue([client_fd, &store = store_] {
            handle_client_connection(client_fd, store);
            close(client_fd);
            LOG("[server] client disconnected (fd=" << client_fd << ")");
        });
    }
}