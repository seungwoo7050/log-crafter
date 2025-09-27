/*
 * Sequence: SEQ0048
 * Track: C++
 * MVP: mvp4
 * Change: Implement the asynchronous persistence manager with log rotation, pruning, and replay helpers.
 * Tests: smoke_cpp_mvp4_persistence
 */
#include "persistence.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>
#include <vector>

namespace logcrafter::cpp {

namespace {
constexpr std::size_t kDefaultMaxFileSize = 10 * 1024 * 1024;
constexpr std::size_t kDefaultMaxFiles = 10;
constexpr std::size_t kLineBufferSize = 2048;
constexpr const char *kCurrentFileName = "current.log";

bool has_log_extension(const char *name) {
    const std::size_t length = std::strlen(name);
    return length >= 4 && std::strcmp(name + length - 4, ".log") == 0;
}

bool collect_log_files(const std::string &directory, bool include_current, std::vector<std::string> &files) {
    DIR *dir = opendir(directory.c_str());
    if (dir == nullptr) {
        return false;
    }

    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (!has_log_extension(entry->d_name)) {
            continue;
        }
        if (!include_current && std::strcmp(entry->d_name, kCurrentFileName) == 0) {
            continue;
        }
        files.emplace_back(entry->d_name);
    }

    closedir(dir);
    std::sort(files.begin(), files.end());

    if (include_current) {
        auto it = std::find(files.begin(), files.end(), kCurrentFileName);
        if (it != files.end() && it + 1 != files.end()) {
            std::rotate(it, it + 1, files.end());
        }
    }

    return true;
}

void trim_newline(char *buffer) {
    std::size_t length = std::strlen(buffer);
    while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
        buffer[length - 1] = '\0';
        --length;
    }
}

} // namespace

PersistenceManager::PersistenceManager()
    : worker_running_(false),
      stop_(false),
      current_file_(nullptr),
      current_size_(0),
      queued_logs_(0),
      persisted_logs_(0),
      failed_logs_(0) {}

PersistenceManager::~PersistenceManager() { shutdown(); }

int PersistenceManager::init(const PersistenceConfig &config) {
    shutdown();

    PersistenceConfig effective = config;
    if (effective.directory.empty()) {
        effective.directory = "./logs";
    }
    if (effective.max_file_size == 0) {
        effective.max_file_size = kDefaultMaxFileSize;
    }
    if (effective.max_files == 0) {
        effective.max_files = kDefaultMaxFiles;
    }

    config_ = effective;
    current_path_ = config_.directory + "/" + kCurrentFileName;

    stop_ = false;
    queued_logs_ = 0;
    persisted_logs_ = 0;
    failed_logs_ = 0;
    queue_.clear();

    if (!ensure_directory()) {
        shutdown();
        return -1;
    }

    if (!open_current_file()) {
        shutdown();
        return -1;
    }

    try {
        worker_ = std::thread(&PersistenceManager::worker_loop, this);
        worker_running_ = true;
    } catch (...) {
        shutdown();
        return -1;
    }

    return 0;
}

void PersistenceManager::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (worker_running_) {
            stop_ = true;
            condition_.notify_all();
        }
    }

    if (worker_running_) {
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    worker_running_ = false;
    stop_ = false;

    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
    close_current_file();
    current_size_ = 0;
}

bool PersistenceManager::enqueue(const std::string &message, std::time_t timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!worker_running_ || stop_) {
        return false;
    }

    queue_.push_back(Entry{timestamp, message});
    ++queued_logs_;
    condition_.notify_one();
    return true;
}

PersistenceStats PersistenceManager::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return PersistenceStats{queued_logs_, persisted_logs_, failed_logs_};
}

int PersistenceManager::replay_existing(const std::function<void(const std::string &, std::time_t)> &callback) {
    if (!callback) {
        errno = EINVAL;
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (current_file_ != nullptr) {
            std::fflush(current_file_);
        }
    }

    std::vector<std::string> files;
    if (!collect_log_files(config_.directory, true, files)) {
        return -1;
    }

    for (const std::string &name : files) {
        const std::string path = config_.directory + "/" + name;
        std::FILE *file = std::fopen(path.c_str(), "r");
        if (file == nullptr) {
            continue;
        }

        char line[kLineBufferSize];
        while (std::fgets(line, sizeof(line), file) != nullptr) {
            trim_newline(line);
            std::size_t offset = 0;
            std::time_t ts = parse_line(line, offset);
            const char *message_start = line + offset;
            std::string message = message_start;
            callback(message, ts);
        }
        std::fclose(file);
    }

    return 0;
}

void PersistenceManager::worker_loop() {
    while (true) {
        Entry entry{};
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this]() { return stop_ || !queue_.empty(); });
            if (stop_ && queue_.empty()) {
                break;
            }
            entry = std::move(queue_.front());
            queue_.pop_front();
        }

        if (!write_entry(entry)) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++failed_logs_;
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            ++persisted_logs_;
        }
    }
}

bool PersistenceManager::ensure_directory() {
    struct stat st {};
    if (stat(config_.directory.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    if (errno != ENOENT) {
        return false;
    }
    return mkdir(config_.directory.c_str(), 0775) == 0;
}

bool PersistenceManager::open_current_file() {
    current_file_ = std::fopen(current_path_.c_str(), "a+");
    if (current_file_ == nullptr) {
        return false;
    }

    if (std::fseek(current_file_, 0, SEEK_END) == 0) {
        long pos = std::ftell(current_file_);
        if (pos >= 0) {
            current_size_ = static_cast<std::size_t>(pos);
        }
    }

    return true;
}

void PersistenceManager::close_current_file() {
    if (current_file_ != nullptr) {
        std::fflush(current_file_);
        std::fclose(current_file_);
        current_file_ = nullptr;
    }
}

bool PersistenceManager::rotate_file(std::time_t timestamp) {
    close_current_file();

    char timestamp_buffer[32];
    format_timestamp(timestamp, timestamp_buffer, sizeof(timestamp_buffer));

    char rotated_name[64];
    if (std::snprintf(rotated_name, sizeof(rotated_name), "%s.log", timestamp_buffer) < 0) {
        return false;
    }

    std::string rotated_path = config_.directory + "/" + rotated_name;
    if (std::rename(current_path_.c_str(), rotated_path.c_str()) != 0) {
        if (errno != ENOENT) {
            return false;
        }
    }

    if (!open_current_file()) {
        return false;
    }

    current_size_ = 0;
    if (!prune_old_files()) {
        // Non-fatal: keep running even if pruning fails.
    }

    return true;
}

bool PersistenceManager::prune_old_files() {
    if (config_.max_files == 0) {
        return true;
    }

    std::vector<std::string> files;
    if (!collect_log_files(config_.directory, false, files)) {
        return false;
    }

    if (files.size() <= config_.max_files) {
        return true;
    }

    const std::size_t excess = files.size() - config_.max_files;
    for (std::size_t i = 0; i < excess; ++i) {
        const std::string path = config_.directory + "/" + files[i];
        if (unlink(path.c_str()) != 0) {
            return false;
        }
    }

    return true;
}

bool PersistenceManager::write_entry(const Entry &entry) {
    if (current_file_ == nullptr && !open_current_file()) {
        return false;
    }

    const std::time_t timestamp = entry.timestamp == static_cast<std::time_t>(0)
                                      ? std::time(nullptr)
                                      : entry.timestamp;

    char timestamp_buffer[32];
    format_timestamp(timestamp, timestamp_buffer, sizeof(timestamp_buffer));
    std::string line = "[";
    line += timestamp_buffer;
    line += "] ";
    line += entry.message;
    line += '\n';

    const std::size_t written = std::fwrite(line.data(), 1, line.size(), current_file_);
    if (written != line.size()) {
        return false;
    }

    std::fflush(current_file_);
    current_size_ += written;

    if (config_.max_file_size > 0 && current_size_ >= config_.max_file_size) {
        if (!rotate_file(timestamp)) {
            return false;
        }
    }

    return true;
}

void PersistenceManager::format_timestamp(std::time_t timestamp, char *buffer, std::size_t capacity) {
    if (capacity == 0) {
        return;
    }

    std::tm tm_value{};
#if defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    if (localtime_r(&timestamp, &tm_value) == nullptr) {
        std::memset(&tm_value, 0, sizeof(tm_value));
        tm_value.tm_year = 70;
        tm_value.tm_mon = 0;
        tm_value.tm_mday = 1;
    }
#else
    std::tm *tmp = std::localtime(&timestamp);
    if (tmp == nullptr) {
        std::memset(&tm_value, 0, sizeof(tm_value));
        tm_value.tm_year = 70;
        tm_value.tm_mon = 0;
        tm_value.tm_mday = 1;
    } else {
        tm_value = *tmp;
    }
#endif

    if (std::strftime(buffer, capacity, "%Y-%m-%d %H:%M:%S", &tm_value) == 0) {
        std::snprintf(buffer, capacity, "1970-01-01 00:00:00");
    }
}

std::time_t PersistenceManager::parse_line(const char *line, std::size_t &offset) {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int consumed = 0;
    if (std::sscanf(line, "[%d-%d-%d %d:%d:%d] %n", &year, &month, &day, &hour, &minute, &second, &consumed) == 6) {
        std::tm tm_value{};
        tm_value.tm_year = year - 1900;
        tm_value.tm_mon = month - 1;
        tm_value.tm_mday = day;
        tm_value.tm_hour = hour;
        tm_value.tm_min = minute;
        tm_value.tm_sec = second;
        tm_value.tm_isdst = -1;
        offset = static_cast<std::size_t>(consumed);
        return std::mktime(&tm_value);
    }
    offset = 0;
    return static_cast<std::time_t>(0);
}

} // namespace logcrafter::cpp
