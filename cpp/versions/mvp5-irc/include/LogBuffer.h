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
#include <map>
#include <functional>

// [SEQUENCE: 172] Log entry with timestamp and IRC integration fields
struct LogEntry {
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    
    // [SEQUENCE: 690] IRC integration fields
    std::string level;      // ERROR, WARNING, INFO, DEBUG
    std::string source;     // Log source (IP, service name, etc.)
    std::string category;   // Log category
    std::map<std::string, std::string> metadata; // Additional metadata
    
    LogEntry(const std::string& msg) 
        : message(msg)
        , timestamp(std::chrono::system_clock::now()) {}
        
    LogEntry(std::string&& msg) 
        : message(std::move(msg))
        , timestamp(std::chrono::system_clock::now()) {}
        
    // [SEQUENCE: 691] Constructor with IRC fields
    LogEntry(const std::string& msg, const std::string& lvl, const std::string& src = "")
        : message(msg)
        , timestamp(std::chrono::system_clock::now())
        , level(lvl)
        , source(src) {}
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
    
    // [SEQUENCE: 294] Enhanced search with parsed query
    std::vector<std::string> searchEnhanced(const class ParsedQuery& query) const;
    
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
    
    // [SEQUENCE: 692] IRC integration - log entry with metadata
    void addLogWithNotification(const LogEntry& entry);
    
    // [SEQUENCE: 693] Register callback for channel patterns
    void registerChannelCallback(const std::string& pattern, 
                                std::function<void(const LogEntry&)> callback);
    void unregisterChannelCallback(const std::string& pattern);

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
    
    // [SEQUENCE: 694] Channel callbacks for IRC integration
    std::map<std::string, std::vector<std::function<void(const LogEntry&)>>> channelCallbacks_;
};

#endif // LOGBUFFER_H