#include "VideoCache.h"
#include "ofMain.h"

VideoCache& VideoCache::get() {
    static VideoCache instance;
    return instance;
}

std::unique_ptr<VideoCache::CachedVideo> VideoCache::acquire(const std::string& path) {
    bool wasCached = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = cache.find(path);
        if (it != cache.end() && it->second.valid) {
            it->second.lastUsed = std::chrono::steady_clock::now();
            it->second.acquireCount++;
            wasCached = true;
            ofLogVerbose("VideoCache") << "Cache metadata hit: " << path
                                        << " (acquire #" << it->second.acquireCount << ")";
        }
    }

    auto cached = std::make_unique<CachedVideo>();
    cached->player = std::make_unique<ofxHapPlayer>();

    if (!cached->player->load(path)) {
        ofLogError("VideoCache") << "Failed to load: " << path;

        // Record failure so subsequent acquires know this path was probed
        {
            std::lock_guard<std::mutex> lock(mutex);
            cache[path] = {false, false, std::chrono::steady_clock::now(), 0};
        }
        return nullptr;
    }

    cached->player->closeAudio();

    // Poll until ready. Cached files are fast (OS cache), first loads are slower.
    int maxRetries = wasCached ? 10 : 50;
    int retry = 0;
    while (!cached->player->isLoaded() && retry < maxRetries) {
        ofSleepMillis(1);
        retry++;
    }

    cached->hasAudio = (cached->player->getAudioOutput() != nullptr);
    cached->loaded = true;

    // Store metadata for future acquires
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto& entry = cache[path];
        if (!entry.valid) {
            entry.hasAudio = cached->hasAudio;
            entry.valid = true;
            entry.lastUsed = std::chrono::steady_clock::now();
            entry.acquireCount = 1;
            ofLogNotice("VideoCache") << "Cached metadata: " << path
                                      << " hasAudio=" << cached->hasAudio
                                      << " (loaded in " << (retry) << "ms)";
        }
    }

    return cached;
}

void VideoCache::release(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = cache.find(path);
    if (it != cache.end()) {
        ofLogNotice("VideoCache") << "Releasing: " << path;
        cache.erase(it);
    }
}

void VideoCache::prune(float maxAgeSeconds) {
    std::lock_guard<std::mutex> lock(mutex);
    auto now = std::chrono::steady_clock::now();
    float maxAgeMs = maxAgeSeconds * 1000.0f;

    for (auto it = cache.begin(); it != cache.end(); ) {
        auto& entry = it->second;
        float ageMs = std::chrono::duration<float, std::milli>(now - entry.lastUsed).count();

        if (ageMs > maxAgeMs) {
            ofLogNotice("VideoCache") << "Pruning: " << it->first
                                      << " (age: " << (ageMs / 1000.0f) << "s, "
                                      << entry.acquireCount << " acquires)";
            it = cache.erase(it);
        } else {
            ++it;
        }
    }
}

void VideoCache::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    cache.clear();
    ofLogNotice("VideoCache") << "Cache cleared";
}