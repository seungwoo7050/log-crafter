#include "ThreadPool.h"
#include <iostream>

// [SEQUENCE: 163] ThreadPool constructor - start worker threads
ThreadPool::ThreadPool(size_t numThreads) : stop_(false) {
    workers_.reserve(numThreads);
    
    // [SEQUENCE: 164] Create and start worker threads
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this] { workerThread(); });
    }
    
    std::cout << "ThreadPool created with " << numThreads << " threads" << std::endl;
}

// [SEQUENCE: 165] ThreadPool destructor - stop all threads
ThreadPool::~ThreadPool() {
    // [SEQUENCE: 166] Signal all threads to stop
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    
    // [SEQUENCE: 167] Join all worker threads
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    std::cout << "ThreadPool destroyed" << std::endl;
}

// [SEQUENCE: 168] Worker thread main loop
void ThreadPool::workerThread() {
    while (true) {
        std::function<void()> task;
        
        // [SEQUENCE: 169] Wait for task or stop signal
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            condition_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });
            
            if (stop_ && tasks_.empty()) {
                return;
            }
            
            // [SEQUENCE: 170] Get task from queue
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        
        // [SEQUENCE: 171] Execute task outside of lock
        task();
    }
}