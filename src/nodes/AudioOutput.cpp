#include "AudioOutput.h"
#include "ofMain.h"
#include "../core/ProcessorCommand.h"
#include "../core/NodeProcessor.h"

using namespace crumble;

class AudioOutputProcessor : public AudioProcessor {
public:
    ControlSlot* masterGainSlot = nullptr;
    ControlSlot* activeSlot = nullptr;

    AudioOutputProcessor() {
        masterGainSlot = getControlPtr(crumble::hashString("gain"));
        activeSlot = getControlPtr(crumble::hashString("active"));
    }
    
    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        
        bool isActive = evalSlot(activeSlot, cycle) > 0.5f;

        if (isActive && inputs[0].processor) {
            inputs[0].processor->pull(buffer, inputs[0].fromOutput, frameCounter, cycle, cycleStep);
        } else {
            buffer.set(0);
            if (!isActive) return;
        }
        
        float* pOut = buffer.getBuffer().data();
        int numChannels = buffer.getNumChannels();
        for (size_t f = 0; f < buffer.getNumFrames(); f++) {
            double sampleCycle = cycle + f * cycleStep;
            float masterVol = evalSlot(masterGainSlot, sampleCycle);
            for (int c = 0; c < numChannels; c++) {
                pOut[f * numChannels + c] *= masterVol;
            }
        }
    }
};

AudioOutput::AudioOutput() {
    type = "audioout";
}

AudioProcessor* AudioOutput::createAudioProcessor() {
    return new AudioOutputProcessor();
}

void AudioOutput::setupProcessor() {
    Node::setupProcessor();
    if (audioProcessor) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::REGISTER_ENDPOINT;
        pushCommand(cmd);
    }
}
