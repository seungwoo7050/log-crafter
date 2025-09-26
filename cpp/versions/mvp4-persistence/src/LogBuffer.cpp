#include "LogBuffer.h"
#include "QueryParser.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

// [SEQUENCE: 186] Constructor initializes buffer with capacity
LogBuffer::LogBuffer(size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0) {
        throw std::invalid_argument("Buffer capacity must be greater than 0");
    }
}

// [SEQUENCE: 187] Push log entry (const reference version)
void LogBuffer::push(const std::string& message) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // [SEQUENCE: 188] Drop oldest if at capacity
    if (buffer_.size() >= capacity_) {
        dropOldest();
    }
    
    // [SEQUENCE: 189] Add new entry
    buffer_.emplace_back(message);
    stats_.totalLogs++;
    
    // [SEQUENCE: 190] Notify waiting consumers
    notEmpty_.notify_one();
}

// [SEQUENCE: 191] Push log entry (move version for efficiency)
void LogBuffer::push(std::string&& message) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (buffer_.size() >= capacity_) {
        dropOldest();
    }
    
    buffer_.emplace_back(std::move(message));
    stats_.totalLogs++;
    
    notEmpty_.notify_one();
}

// [SEQUENCE: 192] Pop log entry (blocking)
std::string LogBuffer::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // [SEQUENCE: 193] Wait for entry if buffer is empty
    notEmpty_.wait(lock, [this] { return !buffer_.empty(); });
    
    // [SEQUENCE: 194] Remove and return oldest entry
    std::string message = std::move(buffer_.front().message);
    buffer_.pop_front();
    
    notFull_.notify_one();
    return message;
}

// [SEQUENCE: 195] Try pop with timeout
bool LogBuffer::tryPop(std::string& message, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // [SEQUENCE: 196] Wait with timeout
    if (!notEmpty_.wait_for(lock, timeout, [this] { return !buffer_.empty(); })) {
        return false;
    }
    
    message = std::move(buffer_.front().message);
    buffer_.pop_front();
    
    notFull_.notify_one();
    return true;
}

// [SEQUENCE: 197] Search for logs containing keyword
std::vector<std::string> LogBuffer::search(const std::string& keyword) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> results;
    
    // [SEQUENCE: 198] Search through all entries
    for (const auto& entry : buffer_) {
        if (entry.message.find(keyword) != std::string::npos) {
            // [SEQUENCE: 199] Format result with timestamp
            auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
            std::stringstream ss;
            ss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] ";
            ss << entry.message;
            results.push_back(ss.str());
        }
    }
    
    return results;
}

// [SEQUENCE: 295] Enhanced search with query parser
std::vector<std::string> LogBuffer::searchEnhanced(const ParsedQuery& query) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> results;
    
    // [SEQUENCE: 296] Search through all entries with enhanced criteria
    for (const auto& entry : buffer_) {
        if (query.matches(entry.message, entry.timestamp)) {
            // [SEQUENCE: 297] Format result with timestamp
            auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
            std::stringstream ss;
            ss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] ";
            ss << entry.message;
            results.push_back(ss.str());
        }
    }
    
    return results;
}

// [SEQUENCE: 200] Get buffer size
size_t LogBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size();
}

// [SEQUENCE: 201] Check if buffer is empty
bool LogBuffer::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.empty();
}

// [SEQUENCE: 202] Check if buffer is full
bool LogBuffer::full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size() >= capacity_;
}

// [SEQUENCE: 203] Clear buffer
void LogBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
    notFull_.notify_all();
}

// [SEQUENCE: 204] Drop oldest entry (must be called with lock held)
void LogBuffer::dropOldest() {
    if (!buffer_.empty()) {
        buffer_.pop_front();
        stats_.droppedLogs++;
    }
}