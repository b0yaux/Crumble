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
    double cycle = 0;       // Musical cycle at the start of the block
    double cycleStep = 0;   // Change in cycle per sample
    int frames = 0;         // Number of samples in the block
    float dt = 0;           // Delta time (for 60fps nodes)
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
    
    // 1. Preparation Phase: The engine pushes timing here.
    // The base class uses this to pre-calculate modulated parameter buffers.
    virtual void prepare(const Context& ctx);

    // 2. Execution Phase
    virtual void update(float dt) {}
    virtual void pullAudio(ofSoundBuffer& buffer, int index = 0) {}

    // Helper for nodes to get a vectorized control stream for any float parameter.
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

    // Patterns (The "Recipe" or mathematical shape assigned to a parameter)
    void setPattern(const std::string& paramName, std::shared_ptr<Pattern<float>> pat) {
        std::lock_guard<std::recursive_mutex> lock(patMutex);
        patterns[paramName] = pat;
    }

    void clearPattern(const std::string& paramName) {
        std::lock_guard<std::recursive_mutex> lock(patMutex);
        patterns.erase(paramName);
    }

    std::shared_ptr<Pattern<float>> getPattern(const std::string& paramName) const {
        std::lock_guard<std::recursive_mutex> lock(patMutex);
        auto it = patterns.find(paramName);
        if (it != patterns.end()) return it->second;
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
    std::unordered_map<std::string, std::shared_ptr<Pattern<float>>> patterns;
    mutable std::recursive_mutex patMutex;

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
