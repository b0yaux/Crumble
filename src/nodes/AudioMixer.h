#pragma once
#include "../core/Node.h"
#include "../core/NodeProcessor.h"

/**
 * AudioMixer sums multiple audio inputs into a single output.
 * It uses reactive expansion (like VideoMixer) to add volume parameters
 * as inputs are connected.
 */
class AudioMixer : public Node {
public:
    AudioMixer();

    void processAudio(ofSoundBuffer& buffer, int index = 0) override;
    std::string getDisplayName() const override;

    crumble::NodeProcessor* createProcessor() override;
    void onParameterChanged(const std::string& paramName) override;
    void onInputConnected(int toInput) override;
    void onInputDisconnected(int toInput) override;

    void addInput();
    void removeInput();

    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;


protected:
    ofParameter<float> masterGain;
    std::vector<std::shared_ptr<ofParameter<float>>> inputGains;
    int numActiveInputs;

    // Cached buffer for processing
    ofSoundBuffer tempBuffer;
};
