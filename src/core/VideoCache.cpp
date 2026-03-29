#include "VideoCache.h"
#include "ofMain.h"

VideoCache& VideoCache::get() {
    static VideoCache instance;
    return instance;
}

std::shared_ptr<VideoCache::CachedVideo> VideoCache::acquire(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = cache.find(path);
        if (it != cache.end()) {
            it->second->lastUsed = std::chrono::steady_clock::now();
            ofLogNotice("VideoCache") << "Cache hit: " << path;
            return it->second;
        }
    }

    ofLogNotice("VideoCache") << "Cache miss, loading: " << path;
    auto cached = std::make_shared<CachedVideo>();
    cached->player = std::make_shared<ofxHapPlayer>();

    if (!cached->player->load(path)) {
        ofLogError("VideoCache") << "Failed to load: " << path;
        return nullptr;
    }

    cached->player->closeAudio();

    // ofxHapPlayer::load() returns before decoding finishes.
    // No async callback API exists, so we poll until ready (blocks main thread).
    // First load of each file stalls ~50-200ms; subsequent accesses are instant (cache hit).
    int retry = 0;
    while (!cached->player->isLoaded() && retry < 50) {
        ofSleepMillis(10);
        retry++;
    }

    cached->hasAudio = (cached->player->getAudioOutput() != nullptr);
    cached->lastUsed = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(mutex);
        cache[path] = cached;
    }

    ofLogNotice("VideoCache") << "Cached video: " << path << " hasAudio=" << cached->hasAudio << " (loaded in " << (retry*10) << "ms)";
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
        float ageMs = std::chrono::duration<float, std::milli>(now - entry->lastUsed).count();

        if (ageMs > maxAgeMs && entry->player.use_count() <= 1) {
            ofLogNotice("VideoCache") << "Pruning unused: " << it->first
                                      << " (age: " << (ageMs / 1000.0f) << "s)";
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

size_t VideoCache::getCacheSize() const {
    std::lock_guard<std::mutex> lock(mutex);
    return cache.size();
}