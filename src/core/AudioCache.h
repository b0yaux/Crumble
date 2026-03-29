#pragma once
#include "ofMain.h"
#include "ofxAudioFile.h"
#include "AssetRegistry.h"
#include "Config.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct DecodedAudio {
    std::vector<float> data;
    size_t numFrames = 0;
    int channels = 0;
    int sampleRate = 0;
};

/**
 * AudioCache — deduplicates loaded audio files in RAM.
 * Two cache types:
 *   - ofxAudioFile: standalone audio files (.wav, .aiff, etc.)
 *   - DecodedAudio: embedded audio decoded from .mov via ffmpeg
 */
class AudioCache {
public:
    AudioCache() = default;

    std::shared_ptr<ofxAudioFile> getAudio(const std::string& path);
    std::shared_ptr<DecodedAudio> getEmbeddedAudio(const std::string& videoPath, int targetRate = 44100);

    void prune(float maxAgeSeconds = 30.0f);
    void clear();

private:
    struct AudioEntry {
        std::shared_ptr<void> asset;
        float lastUsedTime;
    };

    std::map<std::string, AudioEntry> audioFiles;
    std::map<std::string, AudioEntry> embeddedAudio;
    std::mutex mutex;
};
