#include "ofMain.h"
#include "AudioMixer.h"
#include "../core/ProcessorCommand.h"
#include "../core/NodeProcessor.h"

namespace crumble {

class AudioMixerProcessor : public AudioProcessor {
public:
    ControlSlot* masterGainSlot = nullptr;
    std::array<ControlSlot*, MAX_INPUTS> inputGainSlots = {nullptr};

    AudioMixerProcessor() {
        masterGainSlot = getControlPtr(crumble::hashString("gain"));
        sumBuf.allocate(256, 2);
        sumBuf.setSampleRate(44100);
        sumBuf.set(0);
    }

    void addInput(AudioProcessor* p, int toInput, int fromOutput) override {
        AudioProcessor::addInput(p, toInput, fromOutput);
        std::string name = "gain_" + std::to_string(toInput);
        inputGainSlots[toInput] = getControlPtr(crumble::hashString(name.c_str()));
    }

    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        float masterGain = evalSlot(masterGainSlot, cycle);

        for (int i = 0; i < MAX_INPUTS; i++) {
            auto& input = inputs[i];
            if (input.processor) {
                if (sumBuf.getNumFrames() != buffer.getNumFrames() ||
                    sumBuf.getNumChannels() != buffer.getNumChannels()) {
                    sumBuf.allocate(buffer.getNumFrames(), buffer.getNumChannels());
                    sumBuf.setSampleRate(buffer.getSampleRate());
                }
                sumBuf.set(0);

                input.processor->pull(sumBuf, input.fromOutput, frameCounter, cycle, cycleStep);

                float* pSum = sumBuf.getBuffer().data();
                float* pOut = buffer.getBuffer().data();
                int numChannels = buffer.getNumChannels();

                for (size_t f = 0; f < buffer.getNumFrames(); f++) {
                    double sampleCycle = cycle + f * cycleStep;
                    float gain = evalSlot(inputGainSlots[i], sampleCycle);

                    for (int c = 0; c < numChannels; c++) {
                        pOut[f * numChannels + c] += pSum[f * numChannels + c] * gain;
                    }
                }
            }
        }

        if (masterGain != 1.0f) {
            float* pOut = buffer.getBuffer().data();
            for (size_t k = 0; k < buffer.size(); k++) {
                pOut[k] *= masterGain;
            }
        }
    }

private:
    ofSoundBuffer sumBuf;
};

} // namespace crumble

AudioMixer::AudioMixer() {
    type = "audiomix";
    numActiveInputs = 0;
}

crumble::AudioProcessor* AudioMixer::createAudioProcessor() {
    return new crumble::AudioMixerProcessor();
}

void AudioMixer::processAudio(ofSoundBuffer& buffer, int index) {}

std::string AudioMixer::getDisplayName() const {
    return "Audio Mixer";
}

void AudioMixer::onParameterChanged(const std::string& paramName) {
    Node::onParameterChanged(paramName);
}

void AudioMixer::onInputConnected(int index) {
    if (index >= (int)inputGains.size()) {
        int currentSize = inputGains.size();
        for (int i = currentSize; i <= index; i++) {
            std::string name = "gain_" + std::to_string(i);
            auto p = std::make_shared<ofParameter<float>>();
            p->set(name, 0.8f, 0.0f, 1.0f);
            parameters->add(*p);
            inputGains.push_back(p);

            if (audioProcessor) {
                crumble::ProcessorCommand cmd;
                cmd.type = crumble::ProcessorCommand::SET_PARAM;
                cmd.paramHash = crumble::hashString(name.c_str());
                cmd.value = 0.8f;
                pushCommand(cmd);
            }
        }
    }
}

void AudioMixer::onInputDisconnected(int index) {}

void AudioMixer::addInput() {
    numActiveInputs++;
    onInputConnected(numActiveInputs - 1);
}

void AudioMixer::removeInput() {
    if (numActiveInputs > 0) {
        numActiveInputs--;
        if (!inputGains.empty()) {
            parameters->remove(*inputGains.back());
            inputGains.pop_back();
        }
    }
}

ofJson AudioMixer::serialize() const {
    ofJson j;
    ofSerialize(j, *parameters);
    j["numActiveInputs"] = numActiveInputs;
    return j;
}

void AudioMixer::deserialize(const ofJson& json) {
    ofDeserialize(json, *parameters);
    // NOTE: numActiveInputs is serialized but not restored here.
    // Not an issue currently — Lua scripts rebuild the graph on every load,
    // so deserialize() is never called in normal operation.
}
