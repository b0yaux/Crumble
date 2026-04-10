#pragma once
#include "../core/Node.h"
#include <string>

namespace crumble {
    class AudioProcessor;
}

class AudioSource : public Node {
public:
    AudioSource();
    ~AudioSource();
    
    ofParameter<std::string> path;
    ofParameter<std::string> bank;
    ofParameter<bool> playing;
    ofParameter<float> speed;
    ofParameter<float> position;
    ofParameter<bool> loop;
    ofParameter<float> loopSize;

    void processAudio(ofSoundBuffer& buffer, int index = 0) override;
    void update(float dt) override;
    void load(const std::string& path);
    std::string getDisplayName() const override;

    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;
    
    double getRelativePosition() const;
    void setRelativePosition(double pct);

    crumble::AudioProcessor* createAudioProcessor() override;
    void onParameterChanged(const std::string& paramName) override;
    
    bool hasPendingTrigger() const;
    int getPendingTrigger() const;
    void clearPendingTrigger();
    bool hasPendingRest() const;
    void clearPendingRest();
    bool hasPendingPath() const;
    std::string getPendingPath();
    void setMuted(bool muted);
    bool getMuted() const;

private:
    void onPathChanged(std::string& p);
    bool loadEmbedded(const std::string& videoPath);

    std::string loadedPath;
    std::string loadedResolvedPath;  // Dedup: last successfully loaded resolved path
    std::string lastParamPath;  // Dedup: last path set via parameter (not pattern)

    // Async decode retry state — AudioCache returns nullptr on first load,
    // update() polls each frame until the worker finishes decoding.
    std::string pendingDecodePath;
    int pendingDecodeRetries = 0;
    static constexpr int MAX_DECODE_RETRIES = 120;  // ~2s at 60fps
};
