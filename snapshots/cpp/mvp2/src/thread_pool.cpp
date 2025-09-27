/*
 * Sequence: SEQ0038
 * Track: C++
 * MVP: mvp2
 * Change: Implement the MVP2 std::thread worker pool with graceful shutdown and job dispatch.
 * Tests: smoke_cpp_mvp2_thread_pool
 */
#include "thread_pool.hpp"

namespace logcrafter::cpp {

ThreadPool::ThreadPool() : running_(false) {}

ThreadPool::~ThreadPool() {
    stop();
}

int ThreadPool::start(std::size_t thread_count) {
    stop();

    if (thread_count == 0) {
        thread_count = 1;
    }

    running_.store(true, std::memory_order_release);
    try {
        for (std::size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this]() { worker_loop(); });
        }
    } catch (...) {
        stop();
        return -1;
    }

    return 0;
}

void ThreadPool::stop() {
    running_.store(false, std::memory_order_release);
    condition_.notify_all();
    for (std::thread &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<std::function<void()>> empty;
        jobs_.swap(empty);
    }
}

bool ThreadPool::enqueue(std::function<void()> job) {
    if (!running_.load(std::memory_order_acquire)) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.push(std::move(job));
    }

    condition_.notify_one();
    return true;
}

void ThreadPool::worker_loop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this]() {
                return !jobs_.empty() || !running_.load(std::memory_order_acquire);
            });

            if (jobs_.empty()) {
                if (!running_.load(std::memory_order_acquire)) {
                    break;
                }
                continue;
            }

            job = std::move(jobs_.front());
            jobs_.pop();
        }

        if (job) {
            job();
        }
    }
}

} // namespace logcrafter::cpp
