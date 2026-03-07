#include "SpeakersOutput.h"
#include "ofMain.h"
#include "../core/ProcessorCommand.h"
#include "../core/NodeProcessor.h"

using namespace crumble;

class SpeakersOutputProcessor : public AudioProcessor {
public:
    SpeakersOutputProcessor() {
    }
    
    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        if (inputs[0].processor) {
            inputs[0].processor->pull(buffer, inputs[0].fromOutput, frameCounter, cycle, cycleStep);
        } else {
            buffer.set(0);
        }
        
        float* pOut = buffer.getBuffer().data();
        int numChannels = buffer.getNumChannels();
        for (size_t f = 0; f < buffer.getNumFrames(); f++) {
            double sampleCycle = cycle + f * cycleStep;
            float masterVol = evalPattern("masterVolume", sampleCycle);
            for (int c = 0; c < numChannels; c++) {
                pOut[f * numChannels + c] *= masterVol;
            }
        }
    }
};

SpeakersOutput::SpeakersOutput() {
    type = "SpeakersOutput";
    parameters->add(masterVolume.set("masterVolume", 1.0, 0.0, 1.0));
    // NOTE: setupProcessor() is NOT called here.
    // Graph::createNode() calls it after nodeId is assigned.
}

AudioProcessor* SpeakersOutput::createAudioProcessor() {
    return new SpeakersOutputProcessor();
}

void SpeakersOutput::setupProcessor() {
    // Let the base class create the processor and send ADD_NODE
    Node::setupProcessor();
    // Self-register as a session-driven audio endpoint via the wait-free command
    // queue. pushCommand fills in audioProcessor automatically — no direct
    // Session coupling needed here.
    if (audioProcessor) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::REGISTER_ENDPOINT;
        pushCommand(cmd);
    }
}

void SpeakersOutput::onParameterChanged(const std::string& paramName) {
    // Node::onParameterChanged handles all params generically via slotMap.
    Node::onParameterChanged(paramName);
}
