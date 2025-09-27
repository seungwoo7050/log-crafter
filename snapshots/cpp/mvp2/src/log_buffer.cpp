/*
 * Sequence: SEQ0036
 * Track: C++
 * MVP: mvp2
 * Change: Implement the mutex-guarded circular log buffer with snapshot and stats helpers.
 * Tests: smoke_cpp_mvp2_thread_pool
 */
#include "log_buffer.hpp"

#include <algorithm>

namespace logcrafter::cpp {

LogBuffer::LogBuffer()
    : entries_(),
      capacity_(0),
      size_(0),
      head_(0),
      total_logs_(0),
      dropped_logs_(0) {}

void LogBuffer::configure(std::size_t capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    capacity_ = capacity;
    entries_.assign(capacity_, std::string());
    size_ = 0;
    head_ = 0;
    total_logs_ = 0;
    dropped_logs_ = 0;
}

void LogBuffer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_ = 0;
    head_ = 0;
    total_logs_ = 0;
    dropped_logs_ = 0;
    std::fill(entries_.begin(), entries_.end(), std::string());
}

void LogBuffer::push(const std::string &message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (capacity_ == 0) {
        return;
    }

    entries_[head_] = message;
    head_ = (head_ + 1) % capacity_;
    if (size_ == capacity_) {
        ++dropped_logs_;
    } else {
        ++size_;
    }
    ++total_logs_;
}

LogBufferStats LogBuffer::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return LogBufferStats{size_, total_logs_, dropped_logs_};
}

std::vector<std::string> LogBuffer::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> copy;
    copy.reserve(size_);
    if (size_ == 0 || capacity_ == 0) {
        return copy;
    }

    std::size_t start_index = (size_ == capacity_) ? head_ : 0;
    for (std::size_t i = 0; i < size_; ++i) {
        std::size_t index = (start_index + i) % capacity_;
        copy.push_back(entries_[index]);
    }
    return copy;
}

} // namespace logcrafter::cpp
