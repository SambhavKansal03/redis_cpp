#include "server/thread_pool.h"
#include "server/logger.h"

ThreadPool::ThreadPool(size_t num_threads) {
    // Spin up num_threads worker threads.
    // Each thread runs an infinite loop: sleep → wake → pop task → run → repeat.
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;

                {
                    // Lock the queue to safely check/pop.
                    std::unique_lock<std::mutex> lock(queue_mutex_);

                    // wait() atomically releases the lock and blocks the thread.
                    // It wakes when cv_.notify_one/all() is called AND the
                    // condition is true (queue not empty OR shutdown requested).
                    // The lambda is the "predicate" — prevents spurious wakeups.
                    cv_.wait(lock, [this] {
                        return stop_.load() || !tasks_.empty();
                    });

                    // If we're shutting down and there's nothing left, exit.
                    if (stop_.load() && tasks_.empty()) return;

                    // Pop the next task off the front.
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                // Lock is released here — run the task without holding the lock.
                // This is key: other workers can pick up tasks concurrently.
                task();
            }
        });
    }
    LOG("[pool] started " << num_threads << " worker threads");
}

ThreadPool::~ThreadPool() {
    // Signal all workers to stop after finishing their current task.
    stop_.store(true);

    // Wake every thread so they can see stop_ == true and exit their loop.
    cv_.notify_all();

    // join() blocks until each thread exits cleanly.
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    LOG("[pool] all worker threads stopped");
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.push(std::move(task));
    }
    // Wake one sleeping worker to handle this task.
    cv_.notify_one();
}