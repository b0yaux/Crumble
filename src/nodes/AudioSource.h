#pragma once
#include "../core/Node.h"
#include "ofxAudioFile.h"
#include <atomic>
#include <string>
#include <vector>
#include <memory>

namespace crumble {
    class NodeProcessor;
    class AudioProcessor;
}

class AudioSource : public Node {
public:
    AudioSource();
    ~AudioSource();
    
    ofParameter<std::string> path;
    ofParameter<float> speed;
    ofParameter<bool> loop;
    ofParameter<bool> playing;

    void processAudio(ofSoundBuffer& buffer, int index = 0) override;
    void load(const std::string& path);
    void loadEmbedded(const std::string& videoPath);
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
    std::string loadedPath;
    std::shared_ptr<ofxAudioFile> sharedLoader;
    std::vector<float> embeddedAudioData;
    size_t embeddedAudioFrames = 0;
    int embeddedAudioChannels = 0;
};
