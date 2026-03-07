#pragma once

#include <atomic>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Use minimal forward declarations to avoid header hell
class ofSoundBuffer;
class ofTexture;
template<typename T> class ofParameter;
class ofParameterGroup;

// Standard JSON from of
#include "ofJson.h"

#include "Patterns.h"

namespace crumble {
    class NodeProcessor;
    class AudioProcessor;
    class VideoProcessor;
    struct AudioCommand;
}

/**
 * Context represents a "pushed" timing packet for a block of samples.
 */
struct Context {
    double cycle = 0;       
    double cycleStep = 0;   
    int frames = 0;         
    float dt = 0;           
};

/**
 * Control provides a vectorized view of a parameter's values.
 */
struct Control {
    const float* buffer = nullptr;
    float constant = 0.0f;
    bool modulated = false;
    inline float operator[](int i) const { return modulated ? buffer[i] : constant; }
};

class Node {
public:
    Node();
    virtual ~Node();
    
    virtual void prepare(const Context& ctx);
    virtual void update(float dt) {}
    void pullAudio(ofSoundBuffer& buffer, int index = 0);
    virtual ofTexture* getVideoOutput(int index = 0);

    virtual void processAudio(ofSoundBuffer& buffer, int index = 0) {}
    virtual ofTexture* processVideo(int index = 0) { return nullptr; }
    virtual void processAudioBypass(ofSoundBuffer& buffer, int index = 0);
    virtual ofTexture* processVideoBypass(int index = 0);

    Control getControl(ofParameter<float>& param) const;
    virtual void draw() {}
    
    // Shadow Processor Lifecycle
    virtual crumble::AudioProcessor* createAudioProcessor() { return nullptr; }
    virtual crumble::VideoProcessor* createVideoProcessor() { return nullptr; }

    virtual void setupProcessor();
    crumble::AudioProcessor* getAudioProcessor() { return audioProcessor; }
    crumble::VideoProcessor* getVideoProcessor() { return videoProcessor; }

    // The processor pointers are public to allow composite nodes (e.g. AVSampler)
    // to initialize an inner node's processor without the double-ADD_NODE overhead.
    crumble::AudioProcessor* audioProcessor = nullptr;
    crumble::VideoProcessor* videoProcessor = nullptr;

    // Wait-free messaging helper
    void pushCommand(crumble::AudioCommand cmd);

    bool canDraw = false;
    
    // Use raw ofParameter members instead of pointers to stay Crumble-idiomatic
    // but ensure Node.cpp includes the full headers.
    // However, to satisfy forward declarations, we might need a middle ground.
    // For now, let's keep them as pointers to ensure the header compiles.
    std::shared_ptr<ofParameter<int>> drawLayer;
    std::shared_ptr<ofParameter<float>> volume;
    std::shared_ptr<ofParameter<float>> opacity;
    std::shared_ptr<ofParameter<bool>> active; 
    
    virtual std::string getDisplayName() const { return name; }
    void setInputNode(int slot, Node* node);
    Node* getInputNode(int slot) const;
    virtual std::string resolvePath(const std::string& path, const std::string& typeHint = "") const;
    
    std::shared_ptr<ofParameterGroup> parameters;
    std::string name = "unnamed";
    std::string type = "Node";
    class Graph* graph = nullptr;
    int nodeId = -1;
    static std::atomic<int> nextNodeId;

    virtual void modulate(const std::string& paramName, std::shared_ptr<Pattern<float>> pat);
    void clearModulator(const std::string& paramName);
    std::shared_ptr<Pattern<float>> getPattern(const std::string& paramName) const;
    
    bool touched = false;
    virtual ofJson serialize() const;
    virtual void deserialize(const ofJson& json);
    virtual void onInputConnected(int toInput) {}
    virtual void onInputDisconnected(int toInput) {}
    virtual void onParameterChanged(const std::string& paramName);
    
protected:
    std::unordered_map<int, Node*> inputNodes;
    std::unordered_map<std::string, std::shared_ptr<Pattern<float>>> modulators;
    
    mutable std::recursive_mutex modMutex;
    mutable std::unordered_map<std::string, ofSoundBuffer> controlBuffers;
    double lastPreparedCycle = -1.0; 
};

template<typename T>
T getSafeJson(const ofJson& j, const std::string& key, T defaultValue) {
    if (!j.contains(key)) return defaultValue;
    const auto& v = j[key];
    try {
        if constexpr (std::is_same_v<T, std::string>) {
            if (v.is_string()) return v.get<std::string>();
            if (v.is_number()) return v.dump();
            if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
        } else if constexpr (std::is_same_v<T, bool>) {
            if (v.is_boolean()) return v.get<bool>();
            if (v.is_number()) return v.get<float>() > 0.5f;
            if (v.is_string()) {
                std::string s = v.get<std::string>();
                return (s == "true" || s == "1" || s == "TRUE" || s == "ON");
            }
        } else if constexpr (std::is_arithmetic_v<T>) {
            if (v.is_number()) return v.get<T>();
            if (v.is_string()) return (T)std::stod(v.get<std::string>());
            if (v.is_boolean()) return v.get<bool>() ? (T)1 : (T)0;
        }
        return v.get<T>();
    } catch (...) { return defaultValue; }
}
