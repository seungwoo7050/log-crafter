#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <string>
#include <memory>
#include <chrono>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <filesystem>

// [SEQUENCE: 354] Persistence configuration with modern C++
struct PersistenceConfig {
    bool enabled = false;
    std::string log_directory = "./logs";
    size_t max_file_size = 10 * 1024 * 1024;  // 10MB default
    int max_files = 10;
    std::chrono::milliseconds flush_interval{1000};
    bool load_on_startup = true;
};

// [SEQUENCE: 355] Log entry for persistence queue
struct PersistenceEntry {
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    
    PersistenceEntry(std::string msg, std::chrono::system_clock::time_point ts)
        : message(std::move(msg)), timestamp(ts) {}
};

// [SEQUENCE: 356] Persistence statistics
struct PersistenceStats {
    std::atomic<size_t> total_written{0};
    std::atomic<size_t> current_file_size{0};
    std::atomic<int> files_rotated{0};
    std::atomic<int> write_errors{0};
};

// [SEQUENCE: 357] Modern C++ persistence manager with RAII
class PersistenceManager {
public:
    // [SEQUENCE: 358] Constructor with configuration
    explicit PersistenceManager(const PersistenceConfig& config);
    
    // [SEQUENCE: 359] Destructor ensures cleanup
    ~PersistenceManager();
    
    // Disable copy
    PersistenceManager(const PersistenceManager&) = delete;
    PersistenceManager& operator=(const PersistenceManager&) = delete;
    
    // [SEQUENCE: 360] Write log entry asynchronously
    void write(const std::string& message, 
               std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now());
    
    // [SEQUENCE: 361] Force flush pending writes
    void flush();
    
    // [SEQUENCE: 362] Get statistics snapshot
    PersistenceStats getStats() const;
    
    // [SEQUENCE: 363] Load existing logs with callback
    template<typename Callback>
    size_t load(Callback callback);

private:
    // [SEQUENCE: 364] Worker thread function
    void writerThread();
    
    // [SEQUENCE: 365] Rotate log file if needed
    void rotateIfNeeded();
    
    // [SEQUENCE: 366] Write single entry to file
    void writeToFile(const PersistenceEntry& entry);
    
    // [SEQUENCE: 367] Generate rotated filename
    std::string generateRotatedFilename() const;
    
    // [SEQUENCE: 368] Clean up old files
    void cleanupOldFiles();

private:
    PersistenceConfig config_;
    PersistenceStats stats_;
    
    // File handling
    std::ofstream current_file_;
    std::filesystem::path current_path_;
    
    // Async queue
    std::queue<PersistenceEntry> write_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Thread management
    std::thread writer_thread_;
    std::atomic<bool> running_{true};
};

// [SEQUENCE: 369] Template implementation for load
template<typename Callback>
size_t PersistenceManager::load(Callback callback) {
    if (!config_.load_on_startup) {
        return 0;
    }
    
    size_t loaded_count = 0;
    
    try {
        // [SEQUENCE: 370] Iterate through log files using std::filesystem
        for (const auto& entry : std::filesystem::directory_iterator(config_.log_directory)) {
            if (entry.path().extension() == ".log") {
                std::ifstream file(entry.path());
                std::string line;
                
                while (std::getline(file, line)) {
                    // Parse timestamp and message
                    // Format: [YYYY-MM-DD HH:MM:SS] message
                    if (line.length() > 21 && line[0] == '[' && line[20] == ']') {
                        std::string timestamp_str = line.substr(1, 19);
                        std::string message = line.substr(22);
                        
                        // Parse timestamp (simplified for example)
                        std::tm tm = {};
                        std::istringstream ss(timestamp_str);
                        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                        auto timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                        
                        callback(message, timestamp);
                        loaded_count++;
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        // Directory might not exist yet
    }
    
    return loaded_count;
}

#endif // PERSISTENCE_H