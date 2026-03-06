#include "AudioMixer.h"
#include "../core/Graph.h"

AudioMixer::AudioMixer() {
    type = "AudioMixer";
    parameters.add(masterGain.set("masterGain", 1.0, 0.0, 1.0));
    numActiveInputs = 0;
}

void AudioMixer::onInputConnected(int& toInput) {
    // Expand capacity if needed to accommodate the connection
    if (toInput >= numActiveInputs) {
        int newCount = toInput + 1;

        // Add new gain parameters for each new slot
        for (int i = numActiveInputs; i < newCount; i++) {
            std::string name = "gain_" + std::to_string(i);
            auto p = std::make_shared<ofParameter<float>>();
            p->set(name, 0.8f, 0.0f, 1.0f);
            parameters.add(*p);
            inputGains.push_back(p);
        }
        numActiveInputs = newCount;
        ofLogVerbose("AudioMixer") << "Expanded to " << numActiveInputs << " inputs";
    }
}

void AudioMixer::removeInput(int inputIndex) {
    if (inputIndex < 0 || inputIndex >= numActiveInputs) {
        return;
    }

    // 1. Disconnect the graph connection for this input
    if (graph) {
        graph->disconnect(nodeId, inputIndex);
        graph->compactInputIndices(nodeId, inputIndex);
    }

    // 2. Shift gain parameters down
    for (int i = inputIndex; i < numActiveInputs - 1; i++) {
        inputGains[i]->set(inputGains[i + 1]->get());
    }

    // 3. Remove the last gain parameter and shrink
    if (!inputGains.empty()) {
        parameters.remove(*inputGains.back());
        inputGains.pop_back();
    }
    numActiveInputs--;

    ofLogVerbose("AudioMixer") << "Removed input " << inputIndex << " (total: " << numActiveInputs << ")";
}

void AudioMixer::processAudio(ofSoundBuffer& buffer, int index) {
    if (!graph) return;

    // Get optimized reference to connections (No heap allocation)
    const auto& inputs = graph->getInputConnectionsRef(nodeId);
    if (inputs.empty()) return;

    // Ensure tempBuffer matches output buffer size
    if (tempBuffer.getNumFrames() != buffer.getNumFrames() || tempBuffer.getNumChannels() != buffer.getNumChannels()) {
        tempBuffer.allocate(buffer.getNumFrames(), buffer.getNumChannels());
        tempBuffer.setSampleRate(buffer.getSampleRate());
    }

    for (const auto& conn : inputs) {
        int idx = conn.toInput;
        if (idx < 0 || idx >= (int)inputGains.size()) continue;

        Node* source = getInputNode(idx);
        if (source) {
            tempBuffer.set(0);
            source->pullAudio(tempBuffer, conn.fromOutput);

            auto& p = *inputGains[idx];
            Control gainCtrl = getControl(p);
            
            // Get source volume (including modulation)
            Control sourceVolCtrl = source->getControl(source->volume);
            
            for (size_t i = 0; i < buffer.getNumFrames(); i++) {
                float gain = gainCtrl[i] * sourceVolCtrl[i];
                if (gain > 0.0001f) {
                    for (int c = 0; c < buffer.getNumChannels(); c++) {
                        buffer[i * buffer.getNumChannels() + c] += tempBuffer[i * buffer.getNumChannels() + c] * gain;
                    }
                }
            }
        }
    }

    // Apply master gain
    Control masterCtrl = getControl(masterGain);
    if (masterCtrl.modulated || masterGain != 1.0f) {
        for (size_t i = 0; i < buffer.getNumFrames(); i++) {
            float gain = masterCtrl[i];
            for (int c = 0; c < buffer.getNumChannels(); c++) {
                buffer[i * buffer.getNumChannels() + c] *= gain;
            }
        }
    }
}

std::string AudioMixer::getDisplayName() const {
    return "Audio Mixer (" + std::to_string(numActiveInputs) + ")";
}

ofJson AudioMixer::serialize() const {
    ofJson j;
    ofSerialize(j, parameters);
    j["numActiveInputs"] = numActiveInputs;
    return j;
}

void AudioMixer::deserialize(const ofJson& json) {
    ofJson j = json;
    if (j.contains("AudioMixer")) {
        j = j["AudioMixer"];
    } else if (j.contains("group")) {
        j = j["group"];
    } else if (j.contains("params")) {
        j = j["params"];
    }

    // 1. Expand to match saved input count
    int savedCount = getSafeJson<int>(j, "numActiveInputs", 0);

    // Also check if there are any gain_N params even if numActiveInputs is missing
    int maxGainIdx = -1;
    for (auto it = j.begin(); it != j.end(); ++it) {
        std::string key = it.key();
        if (key.find("gain_") == 0) {
            try {
                int idx = std::stoi(key.substr(5));
                if (idx > maxGainIdx) maxGainIdx = idx;
            } catch (...) {}
        }
    }

    int finalCount = std::max(savedCount, maxGainIdx + 1);
    if (finalCount > numActiveInputs) {
        for (int i = numActiveInputs; i < finalCount; i++) {
            int idx = i;
            onInputConnected(idx);
        }
    }

    // 2. Deserialize parameters - handle both old capitalized and new lowercase names
    if (j.contains("MasterGain")) masterGain = getSafeJson<float>(j, "MasterGain", masterGain.get());
    if (j.contains("masterGain")) masterGain = getSafeJson<float>(j, "masterGain", masterGain.get());

    for (int i = 0; i < numActiveInputs; i++) {
        std::string key = "gain_" + std::to_string(i);
        if (j.contains(key)) {
            inputGains[i]->set(getSafeJson<float>(j, key, inputGains[i]->get()));
        }
    }

    ofDeserialize(j, parameters);
}
