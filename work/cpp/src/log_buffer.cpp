/*
 * Sequence: SEQ0052
 * Track: C++
 * MVP: mvp4
 * Change: Support explicit timestamps when inserting entries so persistence replay maintains ordering.
 * Tests: smoke_cpp_mvp4_persistence
 */
#include "log_buffer.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>

namespace logcrafter::cpp {

namespace {

bool safe_localtime(std::time_t timestamp, std::tm &out) {
#if defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    return localtime_r(&timestamp, &out) != nullptr;
#else
    std::tm *result = std::localtime(&timestamp);
    if (result == nullptr) {
        return false;
    }
    out = *result;
    return true;
#endif
}

std::size_t oldest_index(std::size_t head, std::size_t size, std::size_t capacity) {
    if (size == 0) {
        return 0;
    }
    if (size == capacity) {
        return head;
    }
    return 0;
}

} // namespace

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
    entries_.assign(capacity_, Entry{});
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
    for (Entry &entry : entries_) {
        entry.timestamp = 0;
        entry.message.clear();
    }
}

void LogBuffer::push(const std::string &message) {
    push_with_time(message, std::time(nullptr));
}

void LogBuffer::push_with_time(const std::string &message, std::time_t timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (capacity_ == 0) {
        return;
    }

    Entry &slot = entries_[head_];
    slot.timestamp = timestamp == static_cast<std::time_t>(0) ? std::time(nullptr) : timestamp;
    slot.message = message;
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

    std::size_t start_index = oldest_index(head_, size_, capacity_);
    for (std::size_t i = 0; i < size_; ++i) {
        std::size_t index = (start_index + i) % capacity_;
        if (!entries_[index].message.empty()) {
            copy.push_back(entries_[index].message);
        }
    }
    return copy;
}

std::vector<std::string> LogBuffer::execute_query(const QueryRequest &request) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> results;
    if (size_ == 0 || capacity_ == 0) {
        return results;
    }

    std::size_t start_index = oldest_index(head_, size_, capacity_);
    for (std::size_t i = 0; i < size_; ++i) {
        std::size_t index = (start_index + i) % capacity_;
        const Entry &entry = entries_[index];
        if (!entry.message.empty() && entry_matches(entry, request)) {
            std::string formatted = format_entry(entry);
            if (!formatted.empty()) {
                results.push_back(std::move(formatted));
            }
        }
    }

    return results;
}

bool LogBuffer::entry_matches(const Entry &entry, const QueryRequest &request) {
    if (!request.keyword.empty() && entry.message.find(request.keyword) == std::string::npos) {
        return false;
    }

    if (!request.keywords.empty()) {
        if (request.keyword_operator == QueryRequest::Operator::And) {
            for (const std::string &kw : request.keywords) {
                if (!kw.empty() && entry.message.find(kw) == std::string::npos) {
                    return false;
                }
            }
        } else {
            bool any = false;
            for (const std::string &kw : request.keywords) {
                if (!kw.empty() && entry.message.find(kw) != std::string::npos) {
                    any = true;
                    break;
                }
            }
            if (!any) {
                return false;
            }
        }
    }

    if (request.has_regex) {
        try {
            if (!std::regex_search(entry.message, request.regex)) {
                return false;
            }
        } catch (const std::regex_error &) {
            return false;
        }
    }

    if (request.has_time_from && entry.timestamp < request.time_from) {
        return false;
    }

    if (request.has_time_to && entry.timestamp > request.time_to) {
        return false;
    }

    return true;
}

std::string LogBuffer::format_entry(const Entry &entry) {
    std::tm tm_value{};
    if (!safe_localtime(entry.timestamp, tm_value)) {
        std::memset(&tm_value, 0, sizeof(tm_value));
        tm_value.tm_year = 70;
        tm_value.tm_mon = 0;
        tm_value.tm_mday = 1;
    }

    char buffer[32];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_value) == 0) {
        std::snprintf(buffer, sizeof(buffer), "1970-01-01 00:00:00");
    }

    std::ostringstream oss;
    oss << '[' << buffer << "] " << entry.message;
    return oss.str();
}

} // namespace logcrafter::cpp
