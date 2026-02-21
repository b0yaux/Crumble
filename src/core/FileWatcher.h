#pragma once

#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>

/**
 * FileWatcher runs a background thread to monitor file modification times.
 * This prevents blocking the main render thread with synchronous 'stat' calls.
 */
class FileWatcher {
public:
    FileWatcher() : running(false) {}
    ~FileWatcher() { stop(); }

    // Start the background watcher thread
    void start(int pollIntervalMs = 500) {
        if (running) return;
        running = true;
        this->pollIntervalMs = pollIntervalMs;
        watcherThread = std::thread(&FileWatcher::run, this);
    }

    // Stop the background thread
    void stop() {
        running = false;
        if (watcherThread.joinable()) {
            watcherThread.join();
        }
    }

    // Add or update a file to watch
    void watch(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex);
        if (files.find(path) == files.end()) {
            try {
                if (std::filesystem::exists(path)) {
                    files[path] = {std::filesystem::last_write_time(path), false};
                } else {
                    // Watch for future existence
                    files[path] = {std::filesystem::file_time_type::min(), false};
                }
            } catch (...) {
                files[path] = {std::filesystem::file_time_type::min(), false};
            }
        }
    }

    // Check if a file has been modified since last check
    // This clears the dirty flag for the file.
    bool isDirty(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = files.find(path);
        if (it != files.end() && it->second.dirty) {
            it->second.dirty = false;
            return true;
        }
        return false;
    }

private:
    struct FileState {
        std::filesystem::file_time_type lastModified;
        bool dirty;
    };

    void run() {
        while (running) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                for (auto& [path, state] : files) {
                    try {
                        if (std::filesystem::exists(path)) {
                            auto mtime = std::filesystem::last_write_time(path);
                            if (mtime > state.lastModified) {
                                state.lastModified = mtime;
                                state.dirty = true;
                            }
                        }
                    } catch (...) {
                        // File might be locked by another process during write
                        // We'll catch it on the next poll
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        }
    }

    std::map<std::string, FileState> files;
    std::thread watcherThread;
    std::mutex mutex;
    std::atomic<bool> running;
    int pollIntervalMs;
};
