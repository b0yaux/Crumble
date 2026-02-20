#pragma once
#include "ofMain.h"
#include "ofxAudioFile.h"
#include <map>
#include <memory>
#include <mutex>

/**
 * AssetPool decouples media data from the Graph Nodes.
 * It ensures that multiple reloads of the same file path do not 
 * trigger new disk I/O or redundant memory allocations.
 */
class AssetPool {
public:
    // Shared Audio Buffer resource
    struct AudioAsset {
        std::unique_ptr<ofxAudioFile> file;
        std::string path;
        int useCount = 0;
        float lastUsedTime = 0;
    };

    AssetPool() = default;

    // Get a shared audio file reference. 
    // If not in pool, it loads it.
    std::shared_ptr<ofxAudioFile> getAudio(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::string fullPath = ofToDataPath(path);
        
        // 1. Check if already loaded
        auto it = audioAssets.find(fullPath);
        if (it != audioAssets.end()) {
            it->second.lastUsedTime = ofGetElapsedTimef();
            return it->second.sharedFile;
        }

        // 2. Load new
        ofLogNotice("AssetPool") << "Loading new audio asset: " << path;
        auto sharedFile = std::make_shared<ofxAudioFile>();
        sharedFile->load(fullPath);
        
        if (sharedFile->loaded()) {
            AssetEntry entry;
            entry.sharedFile = sharedFile;
            entry.lastUsedTime = ofGetElapsedTimef();
            audioAssets[fullPath] = entry;
            return sharedFile;
        }

        ofLogError("AssetPool") << "Failed to load audio: " << path;
        return nullptr;
    }

    // Optional: cleanup assets that haven't been used for a while
    void prune(float maxAgeSeconds = 30.0f) {
        std::lock_guard<std::mutex> lock(mutex);
        float now = ofGetElapsedTimef();
        
        for (auto it = audioAssets.begin(); it != audioAssets.end(); ) {
            // If the only reference is the pool itself (use_count == 1)
            // and it's old, remove it.
            if (it->second.sharedFile.use_count() <= 1 && (now - it->second.lastUsedTime) > maxAgeSeconds) {
                ofLogNotice("AssetPool") << "Pruning unused asset: " << it->first;
                it = audioAssets.erase(it);
            } else {
                ++it;
            }
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        audioAssets.clear();
    }

private:
    struct AssetEntry {
        std::shared_ptr<ofxAudioFile> sharedFile;
        float lastUsedTime;
    };

    std::map<std::string, AssetEntry> audioAssets;
    std::mutex mutex;
};
