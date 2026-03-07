#pragma once
#include "../core/Node.h"

class SpeakersOutput : public Node {
public:
    SpeakersOutput();
    
    crumble::AudioProcessor* createAudioProcessor() override;
    void setupProcessor() override;
    void onParameterChanged(const std::string& paramName) override;

protected:
    ofParameter<float> masterVolume;
};
