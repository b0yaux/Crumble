#pragma once
#include "ofSoundBuffer.h"
#include <atomic>
#include <array>
#include <string>
#include <memory>
#include <unordered_map>
#include "AudioCommand.h"

namespace crumble {

/**
 * NodeProcessor: The "Shadow" object living on the Audio Thread.
 *
 * Pattern modulation: patternSlots[N] holds a shared_ptr<Pattern<float>>
 * installed via SET_PATTERN. process() receives the block's start cycle and
 * cycleStep so it can call pat->eval(cycle + i*cycleStep) per sample inline.
 * Patterns are stateless pure functions — no locking needed.
 */
class NodeProcessor {
public:
    NodeProcessor() {
        internalBuffer.allocate(1024, 2); 
    }
    virtual ~NodeProcessor() = default;

    // pull() drives the recursive DSP graph traversal.
    // cycle = absolute musical cycle at start of this block
    // cycleStep = cycle increment per sample
    void pull(ofSoundBuffer& buffer, int index = 0, uint64_t frameCounter = 0,
              double cycle = 0.0, double cycleStep = 0.0) {
        if (lastProcessedFrame == frameCounter && frameCounter != 0) {
            copyTo(buffer);
            return;
        }
        
        if (internalBuffer.getNumFrames() != buffer.getNumFrames() || 
            internalBuffer.getNumChannels() != buffer.getNumChannels()) {
            internalBuffer.allocate(buffer.getNumFrames(), buffer.getNumChannels());
        }
        
        internalBuffer.setSampleRate(buffer.getSampleRate());
        internalBuffer.set(0);
        
        process(internalBuffer, index, frameCounter, cycle, cycleStep);
        
        lastProcessedFrame = frameCounter;
        copyTo(buffer);
    }

    // cycle/cycleStep passed in so processors can evaluate patterns sample-accurately.
    virtual void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                         double cycle, double cycleStep) {}
    virtual void handleCommand(const AudioCommand& cmd) {}

    void copyTo(ofSoundBuffer& dest) {
        if (dest.size() == internalBuffer.size()) {
            std::copy(internalBuffer.getBuffer().begin(), internalBuffer.getBuffer().end(),
                      dest.getBuffer().begin());
        }
    }

    ofSoundBuffer internalBuffer;
    uint64_t lastProcessedFrame = 0;

    void addInput(NodeProcessor* p, int toInput, int fromOutput) {
        if (toInput >= 0 && toInput < 16) {
            inputs[toInput] = {p, fromOutput};
        }
    }

    void removeInput(int toInput) {
        if (toInput >= 0 && toInput < 16) {
            inputs[toInput] = {nullptr, 0};
        }
    }

    struct Input {
        NodeProcessor* processor = nullptr;
        int fromOutput = 0;
    };
    std::array<Input, 16> inputs;

    // --- Scalar parameter access (SET_PARAM) ---
    // Name-based storage: valuesMap[name] stores the parameter value.
    // No more index confusion — keys are parameter names directly.
    inline float getParam(const std::string& name) const {
        auto it = valuesMap.find(name);
        if (it != valuesMap.end()) return it->second.load(std::memory_order_relaxed);
        return 0.0f;
    }

    // --- Pattern evaluation (SET_PATTERN) ---
    // Evaluate the pattern installed for a parameter, at the given cycle position.
    // Falls back to the scalar value if no pattern is installed.
    inline float evalPattern(const std::string& name, double cycle) const {
        auto patIt = patternMap.find(name);
        if (patIt != patternMap.end() && patIt->second) {
            return patIt->second->eval(cycle);
        }
        return getParam(name);
    }

    // Direct parameter value storage (name-based, no indices)
    std::unordered_map<std::string, std::atomic<float>> valuesMap;

    // Pattern storage (name-based, no indices)
    std::unordered_map<std::string, std::shared_ptr<Pattern<float>>> patternMap;

    int nodeId = -1;

    bool isSink = false;
};

} // namespace crumble
