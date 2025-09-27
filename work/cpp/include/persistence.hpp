/*
 * Sequence: SEQ0047
 * Track: C++
 * MVP: mvp4
 * Change: Declare the asynchronous persistence manager interface for log rotation and replay support.
 * Tests: smoke_cpp_mvp4_persistence
 */
#ifndef LOGCRAFTER_CPP_PERSISTENCE_HPP
#define LOGCRAFTER_CPP_PERSISTENCE_HPP

#include <cstddef>
#include <cstdio>
#include <ctime>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace logcrafter::cpp {

struct PersistenceConfig {
    std::string directory;
    std::size_t max_file_size;
    std::size_t max_files;
};

struct PersistenceStats {
    unsigned long queued_logs;
    unsigned long persisted_logs;
    unsigned long failed_logs;
};

class PersistenceManager {
public:
    PersistenceManager();
    ~PersistenceManager();

    int init(const PersistenceConfig &config);
    void shutdown();

    bool enqueue(const std::string &message, std::time_t timestamp);
    PersistenceStats stats() const;
    int replay_existing(const std::function<void(const std::string &, std::time_t)> &callback);

private:
    struct Entry {
        std::time_t timestamp;
        std::string message;
    };

    void worker_loop();
    bool ensure_directory();
    bool open_current_file();
    void close_current_file();
    bool rotate_file(std::time_t timestamp);
    bool prune_old_files();
    bool write_entry(const Entry &entry);
    static void format_timestamp(std::time_t timestamp, char *buffer, std::size_t capacity);
    static std::time_t parse_line(const char *line, std::size_t &offset);

    PersistenceConfig config_;
    std::string current_path_;

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<Entry> queue_;
    std::thread worker_;
    bool worker_running_;
    bool stop_;

    std::FILE *current_file_;
    std::size_t current_size_;

    unsigned long queued_logs_;
    unsigned long persisted_logs_;
    unsigned long failed_logs_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_PERSISTENCE_HPP
