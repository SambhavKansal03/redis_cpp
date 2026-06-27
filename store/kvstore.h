#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include <shared_mutex>
#include "store/entry.h"
#include "aof.h"

class KVStore {
public:
    // aof: optional — pass nullptr to disable persistence (e.g. during replay)
    explicit KVStore(AOF* aof = nullptr);

    // Attach AOF after construction (used post-replay in main)
    void set_aof(AOF* aof) { aof_ = aof; }

    bool set(const std::string& key, const std::string& value, long long ttl_ms = 0);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool expire(const std::string& key, long long ttl_ms);
    std::vector<std::string> keys();
    size_t dbsize();
    void flushall();

private:
    std::unordered_map<std::string, Entry> store_;
    mutable std::shared_mutex mutex_;
    AOF* aof_;   // non-owning pointer — AOF outlives KVStore

    void evict_if_expired_unsafe(const std::string& key);
};