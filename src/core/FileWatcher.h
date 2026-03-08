#pragma once

#include "ofMain.h"
#include <map>
#include <vector>
#include <string>
#include <filesystem>

/**
 * FileWatcher runs a background thread to monitor file modification times.
 * Refactored to use ofThread for better integration with the oF lifecycle.
 */
class FileWatcher : public ofThread {
public:
    FileWatcher() : pollIntervalMs(500) {}
    ~FileWatcher() {
        stop();
    }

    void start(int intervalMs = 500) {
        if (isThreadRunning()) return;
        pollIntervalMs = intervalMs;
        startThread();
    }

    void stop() {
        stopThread();
        waitForThread(true);
    }

    // Add a file or directory to monitor.
    // Handles both absolute paths and relative paths (resolved via ofToDataPath).
    void watch(const std::string& path, bool recursive = false) {
        std::lock_guard<std::mutex> lock(mutex);
        std::string absolutePath = ofFilePath::isAbsolute(path) 
                                    ? path 
                                    : ofToDataPath(path);
        watchedRoots[absolutePath] = recursive;
        
        // Initial scan to populate file times
        updatePath(absolutePath, recursive);
    }

    // Returns a list of all files that have changed since the last call
    std::vector<std::string> getChangedFiles() {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<std::string> changed = changedFiles;
        changedFiles.clear();
        return changed;
    }

protected:
    void updatePath(const std::string& path, bool recursive) {
        try {
            if (!std::filesystem::exists(path)) return;

            if (std::filesystem::is_directory(path)) {
                if (recursive) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                        if (entry.is_regular_file()) {
                            checkFile(entry.path().string());
                        }
                    }
                } else {
                    for (const auto& entry : std::filesystem::directory_iterator(path)) {
                        if (entry.is_regular_file()) {
                            checkFile(entry.path().string());
                        }
                    }
                }
            } else {
                checkFile(path);
            }
        } catch (...) {
            // Handle filesystem errors (e.g. permission denied)
        }
    }

    void checkFile(const std::string& filePath) {
        try {
            if (std::filesystem::exists(filePath)) {
                auto currentMTime = std::filesystem::last_write_time(filePath);
                
                auto it = fileTimes.find(filePath);
                if (it != fileTimes.end()) {
                    if (currentMTime > it->second) {
                        it->second = currentMTime;
                        if (std::find(changedFiles.begin(), changedFiles.end(), filePath) == changedFiles.end()) {
                            changedFiles.push_back(filePath);
                        }
                    }
                } else {
                    // First time seeing this file
                    fileTimes[filePath] = currentMTime;
                }
            }
        } catch (...) {
            // File might be locked
        }
    }

    void threadedFunction() override {
        while (isThreadRunning()) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                for (auto const& [path, recursive] : watchedRoots) {
                    updatePath(path, recursive);
                }
            }
            sleep(pollIntervalMs);
        }
    }

private:
    std::map<std::string, bool> watchedRoots; // path -> recursive
    std::map<std::string, std::filesystem::file_time_type> fileTimes;
    std::vector<std::string> changedFiles;
    std::mutex mutex;
    int pollIntervalMs;
};
