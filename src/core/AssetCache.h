#pragma once
#include "ofMain.h"
#include "ofxAudioFile.h"
#include "AssetRegistry.h"
#include "Config.h"
#include <map>
#include <memory>
#include <mutex>
#include <typeindex>
#include <string>
#include <vector>

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
     * If the path is a logical alias (e.g. "birds" or "drums:5"), it is resolved
     * via the AssetRegistry first.
     */
    template<typename T>
    std::shared_ptr<T> get(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex);
        
        // 1. Unified Path Resolution
        std::string absolutePath = path;
        
        // Try to resolve as a logical asset first
        std::string hint = "";
        if constexpr (std::is_same_v<T, ofxAudioFile>) hint = "audio";
        
        std::string resolved = AssetRegistry::get().resolve(path, hint);
        if (!resolved.empty()) {
            absolutePath = resolved;
        } else {
            // Fallback to standard path resolution
            absolutePath = ConfigManager::get().resolvePath(path);
        }
        
        if (absolutePath.empty()) return nullptr;

        auto typeIdx = std::type_index(typeid(T));
        
        // 2. Standard Caching Logic (using resolved path)
        auto& typeCache = caches[typeIdx];
        auto it = typeCache.find(absolutePath);
        if (it != typeCache.end()) {
            it->second.lastUsedTime = ofGetElapsedTimef();
            return std::static_pointer_cast<T>(it->second.asset);
        }

        // 3. Not found: Load and cache
        ofLogNotice("AssetCache") << "Loading [" << typeid(T).name() << "]: " << absolutePath;
        
        auto asset = std::make_shared<T>();
        
        // Politeness check: call load() if the type supports it
        if constexpr (std::is_same_v<T, ofxAudioFile>) {
            asset->load(absolutePath);
            if (!asset->loaded()) {
                ofLogError("AssetCache") << "Failed to load audio: " << absolutePath;
                return nullptr;
            }
        } else {
            // Generic load attempt (e.g. for ofImage)
            if (!asset->load(absolutePath)) {
                ofLogError("AssetCache") << "Failed to load asset: " << absolutePath;
                return nullptr;
            }
        }

        AssetEntry entry;
        entry.asset = asset;
        entry.lastUsedTime = ofGetElapsedTimef();
        typeCache[absolutePath] = entry;
        
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
