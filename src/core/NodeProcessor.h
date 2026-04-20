#pragma once
#include "ofSoundBuffer.h"
#include "ofTexture.h"
#include <atomic>
#include <array>
#include <memory>
#include "ProcessorCommand.h"

namespace crumble {

/**
 * NodeProcessor: The base "Shadow" object living on a background thread.
 */
struct ControlSlot {
    uint32_t hash = 0;
    std::atomic<float> value {0.0f};
    std::shared_ptr<Pattern<float>> pattern;
};

class NodeProcessor {
public:
    static constexpr int MAX_INPUTS = 128;
    static constexpr int MAX_CONTROL_SLOTS = 256;
    
    NodeProcessor() = default;
    virtual ~NodeProcessor() = default;

    virtual void handleCommand(ProcessorCommand& cmd) {}

    // --- Pattern evaluation ---
    
    /**
     * Evaluate a modulation pattern at a specific cycle.
     * Returns the pattern's value at that point in time.
     * If no pattern is set, returns the slot's static value.
     * 
     * @param slot The control slot containing the pattern or static value
     * @param cycle The cycle position (0.0 = start of first cycle, increases monotonically)
     * @return The evaluated value
     */
    inline float evalSlot(ControlSlot* slot, double cycle) {
        if (!slot) return 0.0f;
        if (slot->pattern) return slot->pattern->eval(cycle);
        return slot->value.load(std::memory_order_relaxed);
    }
    
    /**
     * Evaluate a modulation pattern by parameter hash.
     * Convenience wrapper that looks up the slot by hash.
     * 
     * @param hash The parameter name hash (from hashString)
     * @param cycle The cycle position
     * @return The evaluated value
     */
    inline float eval(uint32_t hash, double cycle) {
        return evalSlot(getControlPtr(hash), cycle);
    }
    
    /**
     * Query a trigger pattern for events in a time range.
     * Calls the callback for each event with its sample offset within the buffer.
     * Events are discrete occurrences (notes, triggers) with onset times.
     * 
     * @param slot The control slot containing the trigger pattern
     * @param startBars The start of the query range in bars (monotonically increasing)
     * @param endBars The end of the query range in bars
     * @param samplesPerBar Samples per bar (for converting cycle position to sample offset)
     * @param bufferSize Size of the audio buffer (for clamping offset)
     * @param onEvent Callback: void(const Event<float>& event, int sampleOffset)
     */
    template<typename Callback>
    inline void querySlot(ControlSlot* slot, double startBars, double endBars,
                          double samplesPerBar, int bufferSize, Callback onEvent) {
        if (!slot || !slot->pattern) return;
        auto events = slot->pattern->query(startBars, endBars);
        for (const auto& e : events) {
            double cyclePhase = startBars - std::floor(startBars);
            double relativePos = e.onset - cyclePhase;
            if (relativePos < 0) relativePos += 1.0;
            int offset = static_cast<int>(relativePos * samplesPerBar);
            offset = std::max(0, std::min(bufferSize - 1, offset));
            onEvent(e, offset);
        }
    }
    
    /**
     * Query a trigger pattern by parameter hash.
     * Convenience wrapper that looks up the slot by hash.
     */
    template<typename Callback>
    inline void query(uint32_t hash, double startBars, double endBars,
                      double samplesPerBar, int bufferSize, Callback onEvent) {
        querySlot(getControlPtr(hash), startBars, endBars, samplesPerBar, bufferSize, onEvent);
    }
    
    /**
     * Convenience: evaluate pattern by hash using the current cycle.
     * Useful for simple parameter access in process() functions.
     */
    inline float getParam(uint32_t hash) {
        return eval(hash, currentCycle);
    }

    std::array<ControlSlot, MAX_CONTROL_SLOTS> controls;
    int numControls = 0;

    ControlSlot* getControlPtr(uint32_t hash) {
        for (int i = 0; i < numControls; i++) {
            if (controls[i].hash == hash) return &controls[i];
        }
        if (numControls < MAX_CONTROL_SLOTS) {
            controls[numControls].hash = hash;
            return &controls[numControls++];
        }
        return nullptr;
    }

    void setControl(uint32_t hash, float val, std::shared_ptr<Pattern<float>> pat, std::shared_ptr<Pattern<float>>& displacedOut) {
        ControlSlot* slot = getControlPtr(hash);
        if (slot) {
            slot->value.store(val, std::memory_order_relaxed);
            if (slot->pattern != pat) {
                displacedOut = std::move(slot->pattern);
                slot->pattern = pat;
            }
        }
    }

    int nodeId = -1;
    double currentCycle = 0.0; // Track cycle for getParam fallback
};

/**
 * AudioProcessor: Lives on the Audio Thread.
 */
class AudioProcessor : public NodeProcessor {
public:
    AudioProcessor() {
        internalBuffer.allocate(1024, 2);
        activeSlot = getControlPtr(hashString("active"));
    }
    virtual ~AudioProcessor() = default;

    void pull(ofSoundBuffer& buffer, int index = 0, uint64_t frameCounter = 0,
              double cycle = 0.0, double cycleStep = 0.0) {
        if (lastProcessedFrame == frameCounter && frameCounter != 0) {
            mixTo(buffer);
            return;
        }
        
        currentCycle = cycle; // Update cycle for pattern-aware getParam()
        
        if (internalBuffer.getNumFrames() != buffer.getNumFrames() || 
            internalBuffer.getNumChannels() != buffer.getNumChannels()) {
            internalBuffer.allocate(buffer.getNumFrames(), buffer.getNumChannels());
        }
        
        internalBuffer.setSampleRate(buffer.getSampleRate());
        internalBuffer.set(0);
        
        // Universal active bypass: when inactive, skip process() entirely.
        // internalBuffer stays zero, so mixTo() adds silence to the output.
        if (evalSlot(activeSlot, cycle) < 0.5f) {
            lastProcessedFrame = frameCounter;
            mixTo(buffer);
            return;
        }
        
        process(internalBuffer, index, frameCounter, cycle, cycleStep);
        
        lastProcessedFrame = frameCounter;
        mixTo(buffer);
    }

    virtual void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                         double cycle, double cycleStep) {}

    void mixTo(ofSoundBuffer& dest) {
        if (dest.size() == internalBuffer.size()) {
            float* d = dest.getBuffer().data();
            float* s = internalBuffer.getBuffer().data();
            for (size_t i = 0; i < dest.size(); i++) {
                d[i] += s[i];
            }
        }
    }

    ControlSlot* activeSlot = nullptr;
    ofSoundBuffer internalBuffer;
    uint64_t lastProcessedFrame = 0;
    
    // Shared audio state
    std::atomic<double> playhead{0.0};
    size_t totalSamples = 0;
    int channels = 0;
    const float* data = nullptr;
    std::shared_ptr<void> dataOwner;

    virtual void addInput(AudioProcessor* p, int toInput, int fromOutput) {
        if (toInput >= 0 && toInput < MAX_INPUTS) {
            inputs[toInput] = {p, fromOutput};
        }
    }

    virtual void removeInput(int toInput) {
        if (toInput >= 0 && toInput < MAX_INPUTS) {
            inputs[toInput] = {nullptr, 0};
        }
    }

    struct Input {
        AudioProcessor* processor = nullptr;
        int fromOutput = 0;
    };
    std::array<Input, MAX_INPUTS> inputs;
};

/**
 * VideoProcessor: Evaluated on the Main Thread alongside ScreenOutput.
 *
 * Both processVideo() (called from Session::update) and getOutput() (called
 * from downstream processors) run on the same thread, so no atomics needed.
 * Each subclass owns its output strategy via getOutput():
 *   - VideoSource: returns the player's texture (with YCoCg conversion for HAP Q)
 *   - VideoMixer: returns its composite FBO texture
 *   - VideoOutput: returns nullptr (sink node)
 */
class VideoProcessor : public NodeProcessor {
public:
    VideoProcessor() = default;
    virtual ~VideoProcessor() = default;

    // Called by Session::update() to generate the frame (main thread)
    virtual void processVideo(double cycle, double cycleStep) {}
    
    // Called by downstream processors to fetch the latest texture (main thread)
    virtual ofTexture* getOutput(int index = 0) { 
        return nullptr;
    }

    virtual void addInput(VideoProcessor* p, int toInput, int fromOutput) {
        if (toInput >= 0 && toInput < MAX_INPUTS) {
            inputs[toInput] = {p, fromOutput};
        }
    }

    virtual void removeInput(int toInput) {
        if (toInput >= 0 && toInput < MAX_INPUTS) {
            inputs[toInput] = {nullptr, 0};
        }
    }

    struct Input {
        VideoProcessor* processor = nullptr;
        int fromOutput = 0;
    };
    std::array<Input, MAX_INPUTS> inputs;
};

} // namespace crumble
