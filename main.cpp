#include <iostream>
#include <sstream>
#include <vector>
#include "store/kvstore.h"
#include "server/server.h"
#include "aof.h"

static void replay_line(const std::string& line, KVStore& store) {
    std::istringstream ss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (ss >> token) tokens.push_back(token);
    if (tokens.empty()) return;

    const std::string& cmd = tokens[0];
    if (cmd == "SET" && tokens.size() >= 4) {
        store.set(tokens[1], tokens[2], std::stoll(tokens[3]));
    } else if (cmd == "DEL" && tokens.size() >= 2) {
        store.del(tokens[1]);
    } else if (cmd == "EXPIRE" && tokens.size() >= 3) {
        store.expire(tokens[1], std::stoll(tokens[2]));
    } else if (cmd == "FLUSHALL") {
        store.flushall();
    }
}

int main() {
    // Phase 4: AOF created first, before the store
    AOF aof("appendonly.aof");

    // Step 1: replay existing log with AOF disabled (avoid re-logging)
    KVStore store(nullptr);
    for (const auto& line : aof.load()) {
        replay_line(line, store);
    }

    // Step 2: attach AOF so all future writes are persisted
    store.set_aof(&aof);

    // Step 3: start the server as before
    Server server(6379, store);
    server.run();
    return 0;
}