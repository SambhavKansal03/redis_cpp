#pragma once
#include <string>
#include "store/kvstore.h"
#include "server/thread_pool.h"   // Phase 3

class Server {
public:
    // port:        which port to listen on (6379 — same as Redis)
    // store:       reference to the KVStore — server doesn't own it
    // num_threads: thread pool size (default = hardware core count)
    Server(int port, KVStore& store,
           size_t num_threads = std::thread::hardware_concurrency());

    // Starts the server. Blocks forever — keeps accepting connections.
    void run();

private:
    int port_;
    int server_fd_;
    KVStore& store_;
    ThreadPool pool_;   // Phase 3: replaces sequential handle_client()

    void setup_socket();
};