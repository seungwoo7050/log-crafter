/*
 * Sequence: SEQ0037
 * Track: C++
 * MVP: mvp2
 * Change: Declare the MVP2 std::thread worker pool used to fan out client sessions.
 * Tests: smoke_cpp_mvp2_thread_pool
 */
#ifndef LOGCRAFTER_CPP_THREAD_POOL_HPP
#define LOGCRAFTER_CPP_THREAD_POOL_HPP

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace logcrafter::cpp {

class ThreadPool {
public:
    ThreadPool();
    ~ThreadPool();

    int start(std::size_t thread_count);
    void stop();
    bool enqueue(std::function<void()> job);

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> running_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_THREAD_POOL_HPP
