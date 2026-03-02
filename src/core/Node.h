#pragma once
#include "ofMain.h"
#include <atomic>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "Pattern.h"

/**
 * Context represents a "pushed" timing packet for a block of samples.
 */
struct Context {
    double cycle = 0;       // Current cycle (0.0 to 1.0)
    double cycleStep = 0;   // Step per sample
    int frames = 0;         // Block size
    float dt = 0;           // Delta time
};

/**
 * Control provides a vectorized view of a parameter's values for the current block.
 * This is the "Control Rate" (K-rate) counterpart to audio buffers.
 */
struct Control {
    const float* buffer = nullptr;
    float constant = 0.0f;
    bool modulated = false;
    
    inline float operator[](int i) const {
        return modulated ? buffer[i] : constant;
    }
};

// Minimal base class for all nodes in the graph
class Node {
public:
    Node();
    virtual ~Node() = default;
    
    // Preparation: Pushes timing and pre-calculates modulated parameters.
    // Ensures all patterns are sample-synced before audio processing.
    virtual void prepare(const Context& ctx);

    // Execution Phase
    virtual void update(float dt) {}
    virtual void pullAudio(ofSoundBuffer& buffer, int index = 0) {}

    // Returns a Control view (vector or constant) for a parameter.
    Control getControl(ofParameter<float>& param) const;
    
    // Optional Functional Hooks
    virtual void draw() {}
    
    // Registration Hint
    bool canDraw = false;
    ofParameter<int> drawLayer;
    
    virtual ofTexture* getVideoOutput(int index = 0) { return nullptr; }
    virtual std::string getDisplayName() const { return name; }
    
    ofParameterGroup parameters;
    
    std::string name = "unnamed";
    std::string type = "Node";
    
    class Graph* graph = nullptr;
    int nodeId = -1;
    static std::atomic<int> nextNodeId;

    // Modulators (The act of using a Pattern to change a parameter)
    void modulate(const std::string& paramName, std::shared_ptr<Pattern<float>> pat) {
        std::lock_guard<std::recursive_mutex> lock(modMutex);
        modulators[paramName] = pat;
    }

    void clearModulator(const std::string& paramName) {
        std::lock_guard<std::recursive_mutex> lock(modMutex);
        modulators.erase(paramName);
    }

    std::shared_ptr<Pattern<float>> getPattern(const std::string& paramName) const {
        std::lock_guard<std::recursive_mutex> lock(modMutex);
        auto it = modulators.find(paramName);
        if (it != modulators.end()) return it->second;
        return nullptr;
    }
    
public:
    bool touched = false;
    
    virtual ofJson serialize() const;
    virtual void deserialize(const ofJson& json);
    
    virtual void onInputConnected(int& toInput) {}
    virtual void onInputDisconnected(int& toInput) {}
    virtual void onParameterChanged(const std::string& paramName) {}
    
protected:
    std::unordered_map<std::string, std::shared_ptr<Pattern<float>>> modulators;
    mutable std::recursive_mutex modMutex;

    // Internal buffers for pre-calculated control streams
    mutable std::unordered_map<std::string, ofSoundBuffer> controlBuffers;
};

// Helper to safely get a value from JSON with type-loose conversion
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
    } catch (...) {
        return defaultValue;
    }
}
