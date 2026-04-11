#pragma once
#include "ofMain.h"
#include "ofxHapPlayer.h"
#include <memory>
#include <unordered_map>

/**
 * VideoCache - Singleton metadata cache for video files.
 * Stores validity and audio-presence info so repeated acquires skip
 * redundant file probing. Each acquire() creates an independent
 * ofxHapPlayer with its own playback state and texture.
 */
class VideoCache {
public:
    static VideoCache& get();
    
    struct CachedVideo {
        std::unique_ptr<ofxHapPlayer> player;
        bool hasAudio = false;
        bool loaded = false;
    };
    
    struct CacheEntry {
        bool hasAudio = false;
        bool valid = false;
        std::chrono::steady_clock::time_point lastUsed;
        size_t acquireCount = 0;
    };
    
    /**
     * Create a new independent video player for the given path.
     * Each call returns a unique player with its own playback state.
     * The cache stores metadata (hasAudio, validity) to skip redundant
     * probing on repeated acquires. Returns nullptr if the video failed to load.
     */
    std::unique_ptr<CachedVideo> acquire(const std::string& path);
    
    /**
     * Remove a specific entry from the cache metadata.
     */
    void release(const std::string& path);
    
    /**
     * Evict entries not used in the last maxAgeSeconds.
     */
    void prune(float maxAgeSeconds = 30.0f);
    
    /**
     * Clear all cached videos to free memory.
     */
    void clear();

private:
    VideoCache() = default;
    ~VideoCache() = default;
    VideoCache(const VideoCache&) = delete;
    VideoCache& operator=(const VideoCache&) = delete;
    
    mutable std::unordered_map<std::string, CacheEntry> cache;
    mutable std::mutex mutex;
};