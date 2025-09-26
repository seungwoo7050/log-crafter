#ifndef LOGBUFFER_H
#define LOGBUFFER_H

#include <deque>
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <atomic>
#include <memory>

// [SEQUENCE: 172] Log entry with timestamp
struct LogEntry {
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    
    LogEntry(const std::string& msg) 
        : message(msg)
        , timestamp(std::chrono::system_clock::now()) {}
        
    LogEntry(std::string&& msg) 
        : message(std::move(msg))
        , timestamp(std::chrono::system_clock::now()) {}
};

// [SEQUENCE: 173] Thread-safe circular log buffer using STL
class LogBuffer {
public:
    static constexpr size_t DEFAULT_CAPACITY = 10000;
    
    // [SEQUENCE: 174] Constructor with configurable capacity
    explicit LogBuffer(size_t capacity = DEFAULT_CAPACITY);
    
    // [SEQUENCE: 175] Destructor (RAII handles cleanup)
    ~LogBuffer() = default;
    
    // [SEQUENCE: 176] Push log entry (thread-safe)
    void push(const std::string& message);
    void push(std::string&& message);
    
    // [SEQUENCE: 177] Pop log entry (blocks if empty)
    std::string pop();
    
    // [SEQUENCE: 178] Try pop with timeout
    bool tryPop(std::string& message, std::chrono::milliseconds timeout);
    
    // [SEQUENCE: 179] Search for logs containing keyword
    std::vector<std::string> search(const std::string& keyword) const;
    
    // [SEQUENCE: 180] Get buffer statistics
    size_t size() const;
    bool empty() const;
    bool full() const;
    
    // [SEQUENCE: 181] Get performance statistics
    struct Stats {
        std::atomic<uint64_t> totalLogs{0};
        std::atomic<uint64_t> droppedLogs{0};
    };
    
    // [SEQUENCE: 238] Return stats by value (copying atomic values)
    struct StatsSnapshot {
        uint64_t totalLogs;
        uint64_t droppedLogs;
    };
    StatsSnapshot getStats() const { 
        return { stats_.totalLogs.load(), stats_.droppedLogs.load() };
    }
    
    // [SEQUENCE: 182] Clear buffer
    void clear();

private:
    // [SEQUENCE: 183] Thread-safe buffer implementation
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    
    std::deque<LogEntry> buffer_;
    size_t capacity_;
    
    // [SEQUENCE: 184] Statistics tracking
    mutable Stats stats_;
    
    // [SEQUENCE: 185] Helper to drop oldest entry
    void dropOldest();
};

#endif // LOGBUFFER_H