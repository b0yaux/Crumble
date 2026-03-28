#pragma once
#include "ofMain.h"
#include "ofxHapPlayer.h"
#include <memory>
#include <unordered_map>

/**
 * VideoCache - Singleton cache for loaded videos.
 * Prevents reloading the same video file when switching between samples.
 * Each cached entry holds a fully loaded ofxHapPlayer that can be swapped in.
 */
class VideoCache {
public:
    static VideoCache& get();
    
    struct CachedVideo {
        std::shared_ptr<ofxHapPlayer> player;
        bool hasAudio = false;
        std::chrono::steady_clock::time_point lastUsed;
    };
    
    /**
     * Get or create a cached video player for the given path.
     * Returns nullptr if the video failed to load.
     */
    std::shared_ptr<CachedVideo> acquire(const std::string& path);
    
    /**
     * Release a cached video back to the pool (optional, for LRU management).
     * Currently does nothing but can be used for memory management later.
     */
    void release(const std::string& path);
    
    /**
     * Clear all cached videos to free memory.
     */
    void clear();
    
    /**
     * Get cache statistics for debugging.
     */
    size_t getCacheSize() const;
    
private:
    VideoCache() = default;
    ~VideoCache() = default;
    VideoCache(const VideoCache&) = delete;
    VideoCache& operator=(const VideoCache&) = delete;
    
    mutable std::unordered_map<std::string, std::shared_ptr<CachedVideo>> cache;
    mutable std::mutex mutex;
};