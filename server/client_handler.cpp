#include "server/client_handler.h"
#include <unistd.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <cstring>

// ── Helpers ────────────────────────────────────────────────

static void send_response(int fd, const std::string& msg) {
    std::string response = msg + "\r\n";
    write(fd, response.c_str(), response.size());
}

static std::vector<std::string> parse_command(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

static std::string to_upper(std::string s) {
    for (char& c : s) c = toupper(c);
    return s;
}

// ── Command dispatcher ─────────────────────────────────────

static std::string dispatch(const std::vector<std::string>& tokens, KVStore& store) {
    if (tokens.empty()) return "-ERR empty command";

    std::string cmd = to_upper(tokens[0]);

    // SET key value [ttl_ms]
    if (cmd == "SET") {
        if (tokens.size() < 3) return "-ERR wrong number of args for SET";
        long long ttl = 0;
        if (tokens.size() >= 4) ttl = std::stoll(tokens[3]);
        store.set(tokens[1], tokens[2], ttl);
        return "+OK";
    }

    // GET key
    if (cmd == "GET") {
        if (tokens.size() < 2) return "-ERR wrong number of args for GET";
        auto val = store.get(tokens[1]);
        if (!val.has_value()) return "$-1";
        return "+" + val.value();
    }

    // DEL key
    if (cmd == "DEL") {
        if (tokens.size() < 2) return "-ERR wrong number of args for DEL";
        bool deleted = store.del(tokens[1]);
        return deleted ? ":1" : ":0";
    }

    // EXPIRE key ttl_ms
    if (cmd == "EXPIRE") {
        if (tokens.size() < 3) return "-ERR wrong number of args for EXPIRE";
        bool ok = store.expire(tokens[1], std::stoll(tokens[2]));
        return ok ? ":1" : ":0";
    }

    // DBSIZE
    if (cmd == "DBSIZE") {
        return ":" + std::to_string(store.dbsize());
    }

    // KEYS
    if (cmd == "KEYS") {
        auto ks = store.keys();
        if (ks.empty()) return "*0";
        std::string result = "*" + std::to_string(ks.size());
        for (const auto& k : ks) result += "\r\n+" + k;
        return result;
    }

    // FLUSHALL
    if (cmd == "FLUSHALL") {
        store.flushall();
        return "+OK";
    }

    // PING
    if (cmd == "PING") {
        return "+PONG";
    }

    return "-ERR unknown command '" + tokens[0] + "'";
}

// ── Main client loop ───────────────────────────────────────

void handle_client_connection(int client_fd, KVStore& store) {
    char buffer[4096];

    while (true) {
        memset(buffer, 0, sizeof(buffer));

        ssize_t bytes = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) break;

        std::string line(buffer, bytes);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }

        if (line.empty()) continue;

        std::cout << "[client fd=" << client_fd << "] received: " << line << "\n";

        auto tokens   = parse_command(line);
        auto response = dispatch(tokens, store);

        std::cout << "[client fd=" << client_fd << "] sending: " << response << "\n";
        send_response(client_fd, response);
    }
}