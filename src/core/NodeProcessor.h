#pragma once
#include "ofSoundBuffer.h"
#include "ofTexture.h"
#include "ofFbo.h"
#include <atomic>
#include <array>
#include <string>
#include <memory>
#include <unordered_map>
#include "ProcessorCommand.h"

namespace crumble {

/**
 * NodeProcessor: The base "Shadow" object living on a background thread.
 */
class NodeProcessor {
public:
    static constexpr int MAX_INPUTS = 128;
    
    NodeProcessor() = default;
    virtual ~NodeProcessor() = default;

    virtual void handleCommand(const ProcessorCommand& cmd) {}

    // --- Pattern evaluation (SET_PATTERN) ---
    inline float evalPattern(const std::string& name, double cycle) {
        auto patIt = patternMap.find(name);
        if (patIt != patternMap.end() && patIt->second) {
            return patIt->second->eval(cycle);
        }
        
        // Fallback to static value
        auto it = valuesMap.find(name);
        if (it != valuesMap.end()) {
            return it->second.load(std::memory_order_relaxed);
        }
        return 0.0f;
    }

    // --- Scalar parameter access (SET_PARAM) ---
    inline float getParam(const std::string& name) {
        // 1. Try patterns first using current internal cycle
        auto patIt = patternMap.find(name);
        if (patIt != patternMap.end() && patIt->second) {
            return patIt->second->eval(currentCycle);
        }

        // 2. Fallback to static value
        auto it = valuesMap.find(name);
        if (it != valuesMap.end()) {
            return it->second.load(std::memory_order_relaxed);
        }
        return 0.0f;
    }

    std::unordered_map<std::string, std::atomic<float>> valuesMap;
    std::unordered_map<std::string, std::shared_ptr<Pattern<float>>> patternMap;

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
    }
    virtual ~AudioProcessor() = default;

    void pull(ofSoundBuffer& buffer, int index = 0, uint64_t frameCounter = 0,
              double cycle = 0.0, double cycleStep = 0.0) {
        if (lastProcessedFrame == frameCounter && frameCounter != 0) {
            copyTo(buffer);
            return;
        }
        
        currentCycle = cycle; // Update cycle for pattern-aware getParam()
        
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

    virtual void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                         double cycle, double cycleStep) {}

    void copyTo(ofSoundBuffer& dest) {
        if (dest.size() == internalBuffer.size()) {
            std::copy(internalBuffer.getBuffer().begin(), internalBuffer.getBuffer().end(),
                      dest.getBuffer().begin());
        }
    }

    ofSoundBuffer internalBuffer;
    uint64_t lastProcessedFrame = 0;

    void addInput(AudioProcessor* p, int toInput, int fromOutput) {
        if (toInput >= 0 && toInput < MAX_INPUTS) {
            inputs[toInput] = {p, fromOutput};
        }
    }

    void removeInput(int toInput) {
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
 * VideoProcessor: Lives on the Video Thread with a Shared OpenGL Context.
 */
class VideoProcessor : public NodeProcessor {
public:
    VideoProcessor() = default;
    virtual ~VideoProcessor() = default;

    // Called by the Video Thread to generate the frame
    virtual void processVideo(double cycle, double cycleStep) {}
    
    // Thread-safe fetch of the latest texture for the main thread
    virtual ofTexture* getOutput(int index = 0) { 
        return readyTex.load(std::memory_order_acquire);
    }

    void addInput(VideoProcessor* p, int toInput, int fromOutput) {
        if (toInput >= 0 && toInput < MAX_INPUTS) {
            inputs[toInput] = {p, fromOutput};
        }
    }

    void removeInput(int toInput) {
        if (toInput >= 0 && toInput < MAX_INPUTS) {
            inputs[toInput] = {nullptr, 0};
        }
    }

    struct Input {
        VideoProcessor* processor = nullptr;
        int fromOutput = 0;
    };
    std::array<Input, MAX_INPUTS> inputs;
    
    // Ping-Pong FBO pair for lock-free read/write
    ofTexture tex_A;
    ofTexture tex_B;
    std::atomic<ofTexture*> readyTex { nullptr };
    ofTexture* writeTex = nullptr;
    
    void allocateTextures(int width, int height) {
        // Called on MAIN thread during initialization
        // Textures are shared across OpenGL contexts
        tex_A.allocate(width, height, GL_RGBA);
        tex_B.allocate(width, height, GL_RGBA);
        
        readyTex.store(&tex_A, std::memory_order_release);
        writeTex = &tex_B;
    }

    void swapFbo() {
        ofTexture* oldReady = readyTex.exchange(writeTex, std::memory_order_acq_rel);
        writeTex = oldReady;
    }
};

} // namespace crumble
