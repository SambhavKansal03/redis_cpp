#include "store/kvstore.h"
#include <mutex>

KVStore::KVStore(AOF* aof) : aof_(aof) {}

bool KVStore::set(const std::string& key, const std::string& value, long long ttl_ms) {
    std::unique_lock lock(mutex_);
    if (ttl_ms > 0) {
        store_[key] = Entry(value, ttl_ms);
    } else {
        store_[key] = Entry(value);
    }
    // Log to AOF after the write succeeds
    if (aof_) aof_->log("SET " + key + " " + value + " " + std::to_string(ttl_ms));
    return true;
}

std::optional<std::string> KVStore::get(const std::string& key) {
    std::unique_lock lock(mutex_);
    evict_if_expired_unsafe(key);
    auto it = store_.find(key);
    if (it == store_.end()) return std::nullopt;
    return it->second.value;
    // GET is a read — never logged to AOF
}

bool KVStore::del(const std::string& key) {
    std::unique_lock lock(mutex_);
    bool deleted = store_.erase(key) > 0;
    if (deleted && aof_) aof_->log("DEL " + key);
    return deleted;
}

bool KVStore::expire(const std::string& key, long long ttl_ms) {
    std::unique_lock lock(mutex_);
    evict_if_expired_unsafe(key);
    auto it = store_.find(key);
    if (it == store_.end()) return false;
    it->second.expires_at = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(ttl_ms);
    if (aof_) aof_->log("EXPIRE " + key + " " + std::to_string(ttl_ms));
    return true;
}

std::vector<std::string> KVStore::keys() {
    std::unique_lock lock(mutex_);
    std::vector<std::string> result;
    std::vector<std::string> to_delete;
    for (auto& [k, entry] : store_) {
        if (entry.is_expired()) to_delete.push_back(k);
        else result.push_back(k);
    }
    for (const auto& k : to_delete) store_.erase(k);
    return result;
}

size_t KVStore::dbsize() {
    return keys().size();
}

void KVStore::flushall() {
    std::unique_lock lock(mutex_);
    store_.clear();
    if (aof_) aof_->log("FLUSHALL");
}

void KVStore::evict_if_expired_unsafe(const std::string& key) {
    auto it = store_.find(key);
    if (it == store_.end()) return;
    if (it->second.is_expired()) store_.erase(it);
}