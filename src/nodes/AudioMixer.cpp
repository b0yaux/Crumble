#include "ofMain.h"
#include "AudioMixer.h"
#include "../core/AudioCommand.h"
#include "../core/NodeProcessor.h"

namespace crumble {

class AudioMixerProcessor : public AudioProcessor {
public:
    AudioMixerProcessor() {
        // Pre-allocate to the default buffer size (256 frames, stereo) so the
        // size check in process() is a no-op in the common case.  The guard
        // below still handles any session that uses a non-default buffer size.
        sumBuf.allocate(256, 2);
        sumBuf.setSampleRate(44100);
        sumBuf.set(0);
    }

    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        float masterGain = getParam("masterGain");
        
        for (int i = 0; i < 16; i++) {
            auto& input = inputs[i];
            if (input.processor) {
                // Only reallocate when the host changes buffer geometry (rare).
                if (sumBuf.getNumFrames() != buffer.getNumFrames() ||
                    sumBuf.getNumChannels() != buffer.getNumChannels()) {
                    sumBuf.allocate(buffer.getNumFrames(), buffer.getNumChannels());
                    sumBuf.setSampleRate(buffer.getSampleRate());
                }
                sumBuf.set(0);
                
                // Pass cycle/cycleStep down the graph
                input.processor->pull(sumBuf, input.fromOutput, frameCounter, cycle, cycleStep);
                
                std::string gainParam = "gain_" + std::to_string(i);

                float* pSum = sumBuf.getBuffer().data();
                float* pOut = buffer.getBuffer().data();
                int numChannels = buffer.getNumChannels();
                
                for (size_t f = 0; f < buffer.getNumFrames(); f++) {
                    double sampleCycle = cycle + f * cycleStep;
                    float gain = evalPattern(gainParam, sampleCycle);
                    
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
    ofSoundBuffer sumBuf; // Instance-private summation buffer
};

} // namespace crumble

AudioMixer::AudioMixer() {
    type = "AudioMixer";
    parameters->add(masterGain.set("masterGain", 1.0, 0.0, 4.0));
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
    // Node::onParameterChanged handles all params generically via slotMap.
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
            
            // Register the parameter name and sync the value.
            if (audioProcessor) {
                // Sync initial value using name-based SET_PARAM
                crumble::AudioCommand cmd;
                cmd.type = crumble::AudioCommand::SET_PARAM;
                cmd.slotName = name;
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
}
