#pragma once
#include "../core/Node.h"
#include "../core/ProcessorCommand.h"
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

    bool buildTriggerMap(const std::vector<std::string>& refs, const std::string& bankName);
    void setPendingTriggerBuild(std::vector<std::string> refs, const std::string& bankName);
    
private:
    void onPathChanged(std::string& p);
    bool loadEmbedded(const std::string& videoPath);

    std::string loadedPath;
    std::string loadedResolvedPath;
    std::string lastParamPath;

    std::string pendingDecodePath;

    struct PendingTriggerBuild {
        std::vector<std::string> refs;
        std::string bankName;
    };
    PendingTriggerBuild pendingTriggerBuild;
};
