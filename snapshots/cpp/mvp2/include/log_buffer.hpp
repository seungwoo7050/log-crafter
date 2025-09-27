/*
 * Sequence: SEQ0035
 * Track: C++
 * MVP: mvp2
 * Change: Define the thread-safe circular log buffer for the MVP2 worker pipeline.
 * Tests: smoke_cpp_mvp2_thread_pool
 */
#ifndef LOGCRAFTER_CPP_LOG_BUFFER_HPP
#define LOGCRAFTER_CPP_LOG_BUFFER_HPP

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace logcrafter::cpp {

struct LogBufferStats {
    std::size_t current_size;
    unsigned long total_logs;
    unsigned long dropped_logs;
};

class LogBuffer {
public:
    LogBuffer();

    void configure(std::size_t capacity);
    void reset();

    void push(const std::string &message);
    LogBufferStats stats() const;
    std::vector<std::string> snapshot() const;

private:
    mutable std::mutex mutex_;
    std::vector<std::string> entries_;
    std::size_t capacity_;
    std::size_t size_;
    std::size_t head_;
    unsigned long total_logs_;
    unsigned long dropped_logs_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_LOG_BUFFER_HPP
