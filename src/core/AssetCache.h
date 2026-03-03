#pragma once
#include "ofMain.h"
#include "ofxAudioFile.h"
#include <map>
#include <memory>
#include <mutex>
#include <typeindex>

/**
 * AssetCache provides a centralized, thread-safe deduplication layer for heavy assets.
 * It uses a template-based "Typed Manager" pattern to allow generic access (get<T>)
 * while maintaining type safety and performance.
 */
class AssetCache {
public:
    AssetCache() = default;

    /**
     * Retrieves a shared asset of type T. 
     * If the asset is already loaded, it returns the existing reference.
     * Otherwise, it creates a new instance, loads it, and caches it.
     */
    template<typename T>
    std::shared_ptr<T> get(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::string fullPath = ofToDataPath(path);
        auto typeIdx = std::type_index(typeid(T));
        
        // 1. Check if we have a cache for this type
        auto& typeCache = caches[typeIdx];
        
        // 2. Check if this specific path is already in the cache
        auto it = typeCache.find(fullPath);
        if (it != typeCache.end()) {
            it->second.lastUsedTime = ofGetElapsedTimef();
            return std::static_pointer_cast<T>(it->second.asset);
        }

        // 3. Not found: Load and cache
        ofLogNotice("AssetCache") << "Loading new asset [" << typeid(T).name() << "]: " << path;
        
        auto asset = std::make_shared<T>();
        
        // Politeness check: call load() if the type supports it
        if constexpr (std::is_same_v<T, ofxAudioFile>) {
            asset->load(fullPath);
            if (!asset->loaded()) {
                ofLogError("AssetCache") << "Failed to load audio: " << path;
                return nullptr;
            }
        } else if constexpr (std::is_same_v<T, ofImage>) {
            if (!asset->load(fullPath)) {
                ofLogError("AssetCache") << "Failed to load image: " << path;
                return nullptr;
            }
        }
        // Add more specializations (e.g. ofShader) here as needed.

        AssetEntry entry;
        entry.asset = asset;
        entry.lastUsedTime = ofGetElapsedTimef();
        typeCache[fullPath] = entry;
        
        return asset;
    }

    // Legacy helper for the existing AudioFileSource
    std::shared_ptr<ofxAudioFile> getAudio(const std::string& path) {
        return get<ofxAudioFile>(path);
    }

    /**
     * Removes assets that are no longer referenced by any Node.
     */
    void prune(float maxAgeSeconds = 30.0f) {
        std::lock_guard<std::mutex> lock(mutex);
        float now = ofGetElapsedTimef();
        
        for (auto& [typeIdx, typeCache] : caches) {
            for (auto it = typeCache.begin(); it != typeCache.end(); ) {
                if (it->second.asset.use_count() <= 1 && (now - it->second.lastUsedTime) > maxAgeSeconds) {
                    ofLogNotice("AssetCache") << "Pruning unused asset: " << it->first;
                    it = typeCache.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        caches.clear();
    }

private:
    struct AssetEntry {
        std::shared_ptr<void> asset; // Type-erased shared pointer
        float lastUsedTime;
    };

    // Organized as: [Type] -> [Path] -> AssetEntry
    std::map<std::type_index, std::map<std::string, AssetEntry>> caches;
    std::mutex mutex;
};
