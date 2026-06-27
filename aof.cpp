#include "aof.h"
#include <fstream>
#include <iostream>
#include <vector>

AOF::AOF(const std::string& filepath)
    : filepath_(filepath)
{
    // Open in append mode — never overwrites existing log.
    // ios::app moves the write pointer to the end every time.
    file_.open(filepath_, std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("[aof] failed to open " + filepath_);
    }
    std::cout << "[aof] logging to " << filepath_ << "\n";
}

AOF::~AOF() {
    if (file_.is_open()) file_.close();
}

void AOF::log(const std::string& command) {
    std::unique_lock<std::mutex> lock(mutex_);

    file_ << command << "\n";

    // flush() + sync() forces the OS to write to disk immediately.
    // Without this, data sits in the OS buffer and can be lost on crash.
    file_.flush();
}

std::vector<std::string> AOF::load() {
    std::vector<std::string> lines;

    std::ifstream in(filepath_);
    if (!in.is_open()) {
        // No AOF file yet — fresh start, nothing to replay.
        std::cout << "[aof] no existing log found, starting fresh\n";
        return lines;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }

    std::cout << "[aof] replayed " << lines.size() << " commands from " << filepath_ << "\n";
    return lines;
}