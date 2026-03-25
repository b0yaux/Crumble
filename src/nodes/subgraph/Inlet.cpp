#include "Inlet.h"
#include "ofMain.h"
#include "../../core/NodeProcessor.h"

class InletAudioProcessor : public crumble::AudioProcessor {
public:
    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter, double cycle, double cycleStep) override {
        if (inputs[0].processor) {
            inputs[0].processor->pull(buffer, inputs[0].fromOutput, frameCounter, cycle, cycleStep);
        } else {
            buffer.set(0);
        }
    }
};

class InletVideoProcessor : public crumble::VideoProcessor {
public:
    ofTexture* getOutput(int index = 0) override {
        if (inputs[0].processor) {
            return inputs[0].processor->getOutput(inputs[0].fromOutput);
        }
        return nullptr;
    }
};

Inlet::Inlet() {
    type = "Inlet";
    name = "Inlet";
    parameters->add(inletIndex.set("inletIndex", 0, 0, 16));
}

crumble::AudioProcessor* Inlet::createAudioProcessor() { return new InletAudioProcessor(); }
crumble::VideoProcessor* Inlet::createVideoProcessor() { return new InletVideoProcessor(); }

std::string Inlet::getDisplayName() const {
    return "In " + std::to_string(inletIndex);
}
