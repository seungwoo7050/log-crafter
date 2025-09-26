#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <type_traits>

// [SEQUENCE: 149] Modern C++ thread pool implementation
class ThreadPool {
public:
    // [SEQUENCE: 150] Constructor starts worker threads
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    
    // [SEQUENCE: 151] Destructor joins all threads
    ~ThreadPool();
    
    // [SEQUENCE: 152] Deleted copy operations
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // [SEQUENCE: 153] Enqueue task and return future
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>>;
    
    // [SEQUENCE: 154] Get number of worker threads
    size_t size() const { return workers_.size(); }
    
    // [SEQUENCE: 155] Check if pool is stopping
    bool isStopping() const { return stop_; }

private:
    // [SEQUENCE: 156] Worker thread function
    void workerThread();
    
    // [SEQUENCE: 157] Thread pool state
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    // [SEQUENCE: 158] Synchronization primitives
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

// [SEQUENCE: 159] Template implementation for enqueue
template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
    using return_type = typename std::invoke_result_t<F, Args...>;
    
    // [SEQUENCE: 160] Create packaged task
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    
    // [SEQUENCE: 161] Add task to queue
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        if (stop_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        
        tasks_.emplace([task](){ (*task)(); });
    }
    
    // [SEQUENCE: 162] Notify worker thread
    condition_.notify_one();
    return res;
}

#endif // THREADPOOL_H