#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <vector>

// AOF (Append-Only File) — Phase 4 persistence layer
//
// Every write command (SET, DEL, EXPIRE, FLUSHALL) is appended to
// appendonly.aof as a human-readable line.
//
// On restart, load() replays every line in the file to rebuild
// the store exactly as it was before the crash.
//
// File format (one command per line):
//   SET key value ttl_ms     ← ttl_ms=0 means no expiry
//   DEL key
//   EXPIRE key ttl_ms
//   FLUSHALL

class AOF {
public:
    // Opens appendonly.aof for appending. Creates it if it doesn't exist.
    explicit AOF(const std::string& filepath = "appendonly.aof");
    ~AOF();

    // Append a command string to the log and flush to disk immediately.
    // Thread-safe — protected by a mutex.
    void log(const std::string& command);

    // Replay the log file line by line.
    // Returns each line as a string for the caller to execute.
    // Called once at startup before accepting connections.
    std::vector<std::string> load();

    // Non-copyable
    AOF(const AOF&) = delete;
    AOF& operator=(const AOF&) = delete;

private:
    std::string filepath_;
    std::ofstream file_;
    std::mutex mutex_;   // protects file_ from concurrent writes
};