#include "Outlet.h"
#include "ofMain.h"
#include "../../core/NodeProcessor.h"

class OutletAudioProcessor : public crumble::AudioProcessor {
public:
    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter, double cycle, double cycleStep) override {
        if (inputs[0].processor) {
            inputs[0].processor->pull(buffer, inputs[0].fromOutput, frameCounter, cycle, cycleStep);
        } else {
            buffer.set(0);
        }
    }
};

class OutletVideoProcessor : public crumble::VideoProcessor {
public:
    ofTexture* getOutput(int index = 0) override {
        if (inputs[0].processor) {
            return inputs[0].processor->getOutput(inputs[0].fromOutput);
        }
        return nullptr;
    }
};

Outlet::Outlet() {
    type = "Outlet";
    name = "Outlet";
    parameters->add(outletIndex.set("outletIndex", 0, 0, 16));
}

crumble::AudioProcessor* Outlet::createAudioProcessor() { return new OutletAudioProcessor(); }
crumble::VideoProcessor* Outlet::createVideoProcessor() { return new OutletVideoProcessor(); }

std::string Outlet::getDisplayName() const {
    return "Out " + std::to_string(outletIndex);
}
