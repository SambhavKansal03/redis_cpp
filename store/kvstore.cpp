#include "store/kvstore.h"
#include <mutex>
#include <shared_mutex>

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
    // Phase 1: try with a shared (read) lock — allows concurrent GETs
    {
        std::shared_lock read_lock(mutex_);
        auto it = store_.find(key);
        // Key doesn't exist and can't have expired — return early
        if (it == store_.end()) return std::nullopt;
        // Key exists and hasn't expired — fast path, no upgrade needed
        if (!it->second.is_expired()) return it->second.value;
    }
    // Phase 2: key is expired — upgrade to exclusive lock to evict it
    // We must release the shared_lock first (shared_mutex is not upgradeable)
    {
        std::unique_lock write_lock(mutex_);
        // Re-check after acquiring write lock — another thread may have
        // already evicted it between our two lock acquisitions
        evict_if_expired_unsafe(key);
    }
    return std::nullopt;
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
    // keys() mutates the store (evicts expired entries) so needs a write lock
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
    // Cannot call keys() here — keys() acquires unique_lock and
    // std::shared_mutex is NOT reentrant. Calling keys() from dbsize()
    // would deadlock if dbsize() itself held the lock.
    // Fix: lock once and count inline without calling keys().
    std::unique_lock lock(mutex_);
    size_t count = 0;
    std::vector<std::string> to_delete;
    for (auto& [k, entry] : store_) {
        if (entry.is_expired()) to_delete.push_back(k);
        else ++count;
    }
    for (const auto& k : to_delete) store_.erase(k);
    return count;
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