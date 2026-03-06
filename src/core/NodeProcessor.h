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
        for(auto& v : values) v.store(0.0f);
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
    inline float getParam(int slot) const {
        if (slot < 0 || slot >= 128) return 0.0f;
        return values[slot].load(std::memory_order_relaxed);
    }

    // Name-based slot lookup — returns -1 if not registered.
    inline int getSlot(const std::string& name) const {
        auto it = slotMap.find(name);
        if (it != slotMap.end()) return it->second;
        return -1;
    }

    // Convenience: getParam by name
    inline float getParam(const std::string& name) const {
        return getParam(getSlot(name));
    }

    // --- Pattern evaluation (SET_PATTERN) ---
    // Evaluate the pattern installed at slot, at the given cycle position.
    // Falls back to the scalar value if no pattern is installed.
    inline float evalPattern(int slot, double cycle) const {
        if (slot >= 0 && slot < 128) {
            const auto& pat = patternSlots[slot];
            if (pat) return pat->eval(cycle);
        }
        return getParam(slot);
    }

    inline float evalPattern(const std::string& name, double cycle) const {
        return evalPattern(getSlot(name), cycle);
    }

    // Map from parameter name -> slot index, built by Node::setupProcessor()
    std::unordered_map<std::string, int> slotMap;

    std::array<std::atomic<float>, 128> values;

    // Pattern objects installed by SET_PATTERN — evaluated per-sample by process().
    // Indexed by slot number, same as values[]. nullptr means "use scalar value".
    // Written only by the audio thread (from the command queue drain), read only
    // by process() — no concurrent writes, so plain shared_ptr is safe here.
    std::array<std::shared_ptr<Pattern<float>>, 128> patternSlots;

    int nodeId = -1;

    // Base slots populated by Node::Node() in order:
    //   0: volume, 1: opacity, 2: active, 3: drawLayer
    // Always use getParam("name") / evalPattern("name", cycle) — never hardcode indices.

    bool isSink = false;
};

} // namespace crumble
