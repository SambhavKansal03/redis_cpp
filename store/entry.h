#pragma once
#include <string>
#include <chrono>
#include <optional>

struct Entry {
    std::string value;
    std::optional<std::chrono::steady_clock::time_point> expires_at;

    // Default constructor — required internally by std::unordered_map
    Entry() : value(""), expires_at(std::nullopt) {}

    // No TTL — lives forever
    explicit Entry(std::string val)
        : value(std::move(val)), expires_at(std::nullopt) {}

    // With TTL — expires after ttl_ms milliseconds
    Entry(std::string val, long long ttl_ms)
        : value(std::move(val))
    {
        expires_at = std::chrono::steady_clock::now()
                   + std::chrono::milliseconds(ttl_ms);
    }

    bool is_expired() const {
        if (!expires_at.has_value()) return false;
        return std::chrono::steady_clock::now() > expires_at.value();
    }
};