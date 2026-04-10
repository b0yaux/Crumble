#pragma once
#include "ofMain.h"
#include "ofxAudioFile.h"
#include "AssetRegistry.h"
#include "Config.h"
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

struct DecodedAudio {
    std::vector<float> data;
    size_t numFrames = 0;
    int channels = 0;
    int sampleRate = 0;
};

/**
 * AudioCache — deduplicates loaded audio files in RAM.
 *
 * Two cache types:
 *   - ofxAudioFile: standalone audio files (.wav, .aiff, etc.)
 *   - DecodedAudio: embedded audio decoded from .mov via ffmpeg
 *
 * First-load decoding runs on a background worker thread so the main
 * thread (Lua, video, UI) never blocks. On a cache miss the caller
 * receives nullptr and should retry on the next frame.
 */
class AudioCache {
public:
    AudioCache();
    ~AudioCache();

    std::shared_ptr<ofxAudioFile> getAudio(const std::string& path);
    std::shared_ptr<DecodedAudio> getEmbeddedAudio(const std::string& videoPath, int targetRate = 44100);

    void prune(float maxAgeSeconds = 30.0f);
    void clear();

private:
    // A pending decode job submitted to the worker thread.
    struct DecodeRequest {
        std::string resolvedPath;
        int targetRate = 0;
        bool embedded = true;  // true = embedded video audio, false = standalone audio
    };

    void workerLoop();

    struct AudioEntry {
        std::shared_ptr<void> asset;
        float lastUsedTime;
    };

    // Decoded audio caches (accessed from both main and worker threads)
    std::map<std::string, AudioEntry> audioFiles;
    std::map<std::string, AudioEntry> embeddedAudio;
    std::mutex cacheMutex;

    // Worker thread state
    std::thread worker;
    std::mutex queueMutex;
    std::condition_variable queueCv;
    std::vector<DecodeRequest> workQueue;
    std::unordered_set<std::string> inProgress;
    std::atomic<bool> stopping{false};
};
