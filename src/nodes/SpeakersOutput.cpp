#include "SpeakersOutput.h"
#include "ofMain.h"
#include "../core/Graph.h"
#include "../core/AudioCommand.h"

using namespace crumble;

class SpeakersOutputProcessor : public NodeProcessor {
public:
    SpeakersOutputProcessor() {
        isSink = true;
    }
    
    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        float masterVol = getParam("masterVolume");
        
        if (inputs[0].processor) {
            inputs[0].processor->pull(buffer, inputs[0].fromOutput, frameCounter, cycle, cycleStep);
        } else {
            buffer.set(0);
        }
        
        if (masterVol != 1.0f) {
            buffer *= masterVol;
        }
    }
};

SpeakersOutput::SpeakersOutput() {
    type = "SpeakersOutput";
    parameters->add(masterVolume.set("masterVolume", 1.0, 0.0, 1.0));
    setupProcessor();
}

NodeProcessor* SpeakersOutput::createProcessor() {
    return new SpeakersOutputProcessor();
}

void SpeakersOutput::onParameterChanged(const std::string& paramName) {
    // Node::onParameterChanged handles all params generically via slotMap.
    Node::onParameterChanged(paramName);
}
