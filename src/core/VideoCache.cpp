#include "VideoCache.h"
#include "ofMain.h"

VideoCache& VideoCache::get() {
    static VideoCache instance;
    return instance;
}

std::shared_ptr<VideoCache::CachedVideo> VideoCache::acquire(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Check if already cached
    auto it = cache.find(path);
    if (it != cache.end()) {
        it->second->lastUsed = std::chrono::steady_clock::now();
        ofLogNotice("VideoCache") << "Cache hit: " << path;
        return it->second;
    }
    
    // Load and cache
    ofLogNotice("VideoCache") << "Cache miss, loading: " << path;
    auto cached = std::make_shared<CachedVideo>();
    cached->player = std::make_shared<ofxHapPlayer>();
    
    if (!cached->player->load(path)) {
        ofLogError("VideoCache") << "Failed to load: " << path;
        return nullptr;
    }
    
    cached->player->closeAudio();
    
    // Give the demuxer a moment to find streams (it's async)
    // We poll for up to 500ms
    int retry = 0;
    while (!cached->player->isLoaded() && retry < 50) {
        ofSleepMillis(10);
        retry++;
    }
    
    cached->hasAudio = (cached->player->getAudioOutput() != nullptr);
    cached->lastUsed = std::chrono::steady_clock::now();
    cache[path] = cached;
    
    ofLogNotice("VideoCache") << "Cached video: " << path << " hasAudio=" << cached->hasAudio << " (loaded in " << (retry*10) << "ms)";
    return cached;
}

void VideoCache::release(const std::string& path) {
    // Currently a no-op, but could implement LRU eviction here
}

void VideoCache::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    cache.clear();
    ofLogNotice("VideoCache") << "Cache cleared";
}

size_t VideoCache::getCacheSize() const {
    std::lock_guard<std::mutex> lock(mutex);
    return cache.size();
}