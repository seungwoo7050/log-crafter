#include "Persistence.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// [SEQUENCE: 371] Constructor creates directory and starts writer thread
PersistenceManager::PersistenceManager(const PersistenceConfig& config)
    : config_(config) {
    
    if (config_.enabled) {
        // [SEQUENCE: 372] Create log directory using std::filesystem
        try {
            std::filesystem::create_directories(config_.log_directory);
        } catch (const std::filesystem::filesystem_error& e) {
            throw std::runtime_error("Failed to create log directory: " + std::string(e.what()));
        }
        
        // [SEQUENCE: 373] Open current log file
        current_path_ = std::filesystem::path(config_.log_directory) / "current.log";
        current_file_.open(current_path_, std::ios::app);
        if (!current_file_) {
            throw std::runtime_error("Failed to open log file: " + current_path_.string());
        }
        
        // [SEQUENCE: 374] Get current file size
        current_file_.seekp(0, std::ios::end);
        stats_.current_file_size = current_file_.tellp();
        
        // [SEQUENCE: 375] Start writer thread
        writer_thread_ = std::thread(&PersistenceManager::writerThread, this);
    }
}

// [SEQUENCE: 376] Destructor stops thread and flushes
PersistenceManager::~PersistenceManager() {
    if (config_.enabled) {
        // [SEQUENCE: 377] Signal thread to stop
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            running_ = false;
        }
        queue_cv_.notify_all();
        
        // [SEQUENCE: 378] Wait for thread to finish
        if (writer_thread_.joinable()) {
            writer_thread_.join();
        }
        
        // [SEQUENCE: 379] Close file
        if (current_file_.is_open()) {
            current_file_.close();
        }
    }
}

// [SEQUENCE: 380] Queue message for async write
void PersistenceManager::write(const std::string& message, 
                              std::chrono::system_clock::time_point timestamp) {
    if (!config_.enabled) return;
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        write_queue_.emplace(message, timestamp);
    }
    queue_cv_.notify_one();
}

// [SEQUENCE: 381] Force flush pending writes
void PersistenceManager::flush() {
    if (!config_.enabled) return;
    
    // Signal writer thread
    queue_cv_.notify_one();
    
    // Wait a bit for flush
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// [SEQUENCE: 382] Get statistics snapshot
PersistenceStats PersistenceManager::getStats() const {
    return PersistenceStats{
        stats_.total_written.load(),
        stats_.current_file_size.load(),
        stats_.files_rotated.load(),
        stats_.write_errors.load()
    };
}

// [SEQUENCE: 383] Writer thread main loop
void PersistenceManager::writerThread() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // [SEQUENCE: 384] Wait for entries or timeout
        queue_cv_.wait_for(lock, config_.flush_interval,
            [this] { return !write_queue_.empty() || !running_; });
        
        // [SEQUENCE: 385] Process all queued entries
        std::queue<PersistenceEntry> entries_to_write;
        entries_to_write.swap(write_queue_);
        lock.unlock();
        
        // [SEQUENCE: 386] Write entries to file
        while (!entries_to_write.empty()) {
            writeToFile(entries_to_write.front());
            entries_to_write.pop();
        }
        
        // [SEQUENCE: 387] Flush to disk
        if (current_file_.is_open()) {
            current_file_.flush();
        }
    }
    
    // [SEQUENCE: 388] Final flush on shutdown
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!write_queue_.empty()) {
        writeToFile(write_queue_.front());
        write_queue_.pop();
    }
    if (current_file_.is_open()) {
        current_file_.flush();
    }
}

// [SEQUENCE: 389] Write single entry to file
void PersistenceManager::writeToFile(const PersistenceEntry& entry) {
    if (!current_file_.is_open()) {
        stats_.write_errors++;
        return;
    }
    
    try {
        // [SEQUENCE: 390] Format timestamp using std::put_time
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        current_file_ << "[" 
                     << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                     << "] " << entry.message << std::endl;
        
        stats_.total_written++;
        
        // [SEQUENCE: 391] Update file size
        size_t new_size = current_file_.tellp();
        stats_.current_file_size = new_size;
        
        // [SEQUENCE: 392] Check if rotation needed
        if (new_size >= config_.max_file_size) {
            rotateIfNeeded();
        }
        
    } catch (const std::exception& e) {
        stats_.write_errors++;
    }
}

// [SEQUENCE: 393] Rotate log file
void PersistenceManager::rotateIfNeeded() {
    if (!current_file_.is_open()) return;
    
    // [SEQUENCE: 394] Close current file
    current_file_.close();
    
    // [SEQUENCE: 395] Generate new filename
    std::string new_name = generateRotatedFilename();
    std::filesystem::path new_path = std::filesystem::path(config_.log_directory) / new_name;
    
    try {
        // [SEQUENCE: 396] Rename current.log to timestamped file
        std::filesystem::rename(current_path_, new_path);
        stats_.files_rotated++;
        
        // [SEQUENCE: 397] Open new current.log
        current_file_.open(current_path_, std::ios::app);
        stats_.current_file_size = 0;
        
        // [SEQUENCE: 398] Clean up old files if needed
        cleanupOldFiles();
        
    } catch (const std::filesystem::filesystem_error& e) {
        stats_.write_errors++;
        // Try to reopen current file
        current_file_.open(current_path_, std::ios::app);
    }
}

// [SEQUENCE: 399] Generate timestamped filename
std::string PersistenceManager::generateRotatedFilename() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d-%H%M%S") << ".log";
    return ss.str();
}

// [SEQUENCE: 400] Clean up old log files
void PersistenceManager::cleanupOldFiles() {
    if (config_.max_files <= 0) return;
    
    try {
        // [SEQUENCE: 401] Collect all log files with timestamps
        std::vector<std::filesystem::path> log_files;
        
        for (const auto& entry : std::filesystem::directory_iterator(config_.log_directory)) {
            if (entry.is_regular_file() && 
                entry.path().extension() == ".log" && 
                entry.path().filename() != "current.log") {
                log_files.push_back(entry.path());
            }
        }
        
        // [SEQUENCE: 402] Sort by modification time (oldest first)
        std::sort(log_files.begin(), log_files.end(),
            [](const auto& a, const auto& b) {
                return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
            });
        
        // [SEQUENCE: 403] Remove oldest files if exceeding limit
        while (log_files.size() >= static_cast<size_t>(config_.max_files)) {
            std::filesystem::remove(log_files.front());
            log_files.erase(log_files.begin());
        }
        
    } catch (const std::filesystem::filesystem_error& e) {
        // Log cleanup errors but don't fail
    }
}