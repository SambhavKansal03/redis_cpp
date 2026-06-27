#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

// ThreadPool — Phase 3 concurrency layer
//
// How it works:
//   1. Constructor spins up N worker threads, each running a loop that
//      blocks on a condition variable waiting for work.
//   2. enqueue() pushes a task (any callable) onto a shared queue and
//      wakes one sleeping worker via notify_one().
//   3. The woken worker pops the task, runs it, then goes back to sleep.
//   4. Destructor sets stop_ = true, wakes ALL threads, and joins them.
//
// This avoids the cost of creating/destroying a thread per client —
// threads are reused across connections.

class ThreadPool {
public:
    // num_threads: how many worker threads to keep alive.
    // A good default is std::thread::hardware_concurrency().
    explicit ThreadPool(size_t num_threads);

    // Destructor: signals workers to stop and waits for all of them.
    ~ThreadPool();

    // Push a task onto the queue. The next available worker will run it.
    // Task can be any callable: lambda, std::function, function pointer, etc.
    void enqueue(std::function<void()> task);

    // Non-copyable — threads can't be copied.
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    std::vector<std::thread> workers_;          // the actual OS threads
    std::queue<std::function<void()>> tasks_;   // pending work items

    std::mutex queue_mutex_;             // protects tasks_ queue
    std::condition_variable cv_;         // workers sleep on this
    std::atomic<bool> stop_{false};      // set to true on shutdown
};