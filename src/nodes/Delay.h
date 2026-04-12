#pragma once
#include "../core/Node.h"

class Delay : public Node {
public:
    Delay();

    ofParameter<float> time;
    ofParameter<float> feedback;
    ofParameter<float> wet;

    crumble::AudioProcessor* createAudioProcessor() override;
    std::string getDisplayName() const override { return "Delay"; }
};
