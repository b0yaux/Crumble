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

    // Add a file to monitor
    void watch(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex);
        std::string absolutePath = ofToDataPath(path);
        if (files.find(absolutePath) == files.end()) {
            try {
                if (std::filesystem::exists(absolutePath)) {
                    files[absolutePath] = std::filesystem::last_write_time(absolutePath);
                } else {
                    files[absolutePath] = std::filesystem::file_time_type::min();
                }
            } catch (...) {
                files[absolutePath] = std::filesystem::file_time_type::min();
            }
        }
    }

    // Returns a list of all files that have changed since the last call
    std::vector<std::string> getChangedFiles() {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<std::string> changed = changedFiles;
        changedFiles.clear();
        return changed;
    }

protected:
    void threadedFunction() override {
        while (isThreadRunning()) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                for (auto& [path, lastMTime] : files) {
                    try {
                        if (std::filesystem::exists(path)) {
                            auto currentMTime = std::filesystem::last_write_time(path);
                            if (currentMTime > lastMTime) {
                                lastMTime = currentMTime;
                                // Only add to queue if not already there
                                if (std::find(changedFiles.begin(), changedFiles.end(), path) == changedFiles.end()) {
                                    changedFiles.push_back(path);
                                }
                            }
                        }
                    } catch (...) {
                        // File locked during write, will catch on next loop
                    }
                }
            }
            sleep(pollIntervalMs);
        }
    }

private:
    std::map<std::string, std::filesystem::file_time_type> files;
    std::vector<std::string> changedFiles;
    int pollIntervalMs;
};
