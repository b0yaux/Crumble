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
     * Remove a specific entry from the cache.
     * Safe to call even if a VideoSource still references the player
     * — the shared_ptr keeps it alive until the source releases it.
     */
    void release(const std::string& path);
    
    /**
     * Evict entries not used in the last maxAgeSeconds and not referenced
     * by any VideoSource (player use_count == 1 means cache-only reference).
     */
    void prune(float maxAgeSeconds = 30.0f);
    
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