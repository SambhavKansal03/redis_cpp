#pragma once
#include <iostream>
#include <mutex>
#include <string>

// Global log mutex — all threads must hold this before writing to std::cout.
// Prevents interleaved output when multiple worker threads log simultaneously.
//
// Usage:
//   LOG("[server] client connected fd=" << fd);
//
// The macro locks, streams, and unlocks in one expression.
// '\n' is used instead of std::endl — endl flushes the buffer on every call
// which is expensive; we only need the newline.

inline std::mutex g_log_mutex;

#define LOG(msg) \
    do { \
        std::lock_guard<std::mutex> _lg(g_log_mutex); \
        std::cout << msg << '\n'; \
    } while(0)