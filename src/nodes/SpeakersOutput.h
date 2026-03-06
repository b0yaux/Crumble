#pragma once
#include "../core/Node.h"
#include "../core/NodeProcessor.h"

class SpeakersOutput : public Node {
public:
    SpeakersOutput();
    
    crumble::NodeProcessor* createProcessor() override;
    void onParameterChanged(const std::string& paramName) override;

protected:
    ofParameter<float> masterVolume;
};
