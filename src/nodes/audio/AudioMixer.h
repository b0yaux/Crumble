#pragma once
#include "../../core/Node.h"
#include "../../core/Graph.h"

/**
 * AudioMixer sums multiple audio inputs into a single output.
 * It uses reactive expansion (like VideoMixer) to add volume parameters
 * as inputs are connected.
 */
class AudioMixer : public Node {
public:
    AudioMixer() {
        type = "AudioMixer";
        parameters.setName("AudioMixer");
        parameters.add(masterGain.set("MasterGain", 1.0, 0.0, 1.0));
        
        // Internal state
        numActiveInputs = 0;
    }

    void onInputConnected(int& toInput) override {
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

    void audioOut(ofSoundBuffer& buffer) override {
        if (!graph) return;

        // Get all connections to this node
        auto inputs = graph->getInputConnections(nodeIndex);
        if (inputs.empty()) return;

        // Use a thread-local or temporary buffer for sub-mixes to avoid allocations in audio thread
        // For simplicity and multi-threading safety, we'll use a local buffer
        // (In a future optimization, we can double-buffer this)
        tempBuffer.allocate(buffer.getNumFrames(), buffer.getNumChannels());
        tempBuffer.setSampleRate(buffer.getSampleRate());

        for (const auto& conn : inputs) {
            int idx = conn.toInput;
            if (idx < 0 || idx >= (int)inputGains.size()) continue;

            Node* source = graph->getNode(conn.fromNode);
            if (source) {
                tempBuffer.set(0);
                source->audioOut(tempBuffer);
                
                float gain = inputGains[idx]->get();
                if (gain > 0) {
                    for (size_t i = 0; i < buffer.size(); i++) {
                        buffer[i] += tempBuffer[i] * gain;
                    }
                }
            }
        }

        // Apply master gain
        if (masterGain != 1.0f) {
            buffer *= (float)masterGain;
        }
    }

    std::string getDisplayName() const override {
        return "Audio Mixer (" + std::to_string(numActiveInputs) + ")";
    }

protected:
    ofParameter<float> masterGain;
    std::vector<std::shared_ptr<ofParameter<float>>> inputGains;
    int numActiveInputs;
    
    // Cached buffer for processing
    ofSoundBuffer tempBuffer;
};
