/*
 * Sequence: SEQ0051
 * Track: C++
 * MVP: mvp4
 * Change: Allow timestamp-controlled log ingestion so persistence replay can restore historical entries.
 * Tests: smoke_cpp_mvp4_persistence
 */
#ifndef LOGCRAFTER_CPP_LOG_BUFFER_HPP
#define LOGCRAFTER_CPP_LOG_BUFFER_HPP

#include <cstddef>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#include "query_parser.hpp"

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
    void push_with_time(const std::string &message, std::time_t timestamp);
    LogBufferStats stats() const;
    std::vector<std::string> snapshot() const;
    std::vector<std::string> execute_query(const QueryRequest &request) const;

private:
    struct Entry {
        std::time_t timestamp;
        std::string message;
    };

    static bool entry_matches(const Entry &entry, const QueryRequest &request);
    static std::string format_entry(const Entry &entry);

    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
    std::size_t capacity_;
    std::size_t size_;
    std::size_t head_;
    unsigned long total_logs_;
    unsigned long dropped_logs_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_LOG_BUFFER_HPP
