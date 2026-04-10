#pragma once
#include "../core/Node.h"
#include <string>
#include <vector>

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

    void setResolvedPaths(const std::vector<std::string>& paths);

private:
    void onPathChanged(std::string& p);
    bool loadEmbedded(const std::string& videoPath);

    std::string loadedPath;
    std::string loadedResolvedPath;
    std::string lastParamPath;

    std::string pendingDecodePath;
    int pendingDecodeRetries = 0;
    static constexpr int MAX_DECODE_RETRIES = 120;

    std::vector<std::string> resolvedPaths;
};
