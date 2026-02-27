#pragma once
#include "ofMain.h"
#include <atomic>

// Minimal base class for all nodes in the graph
// No Port objects, no ConnectionManager - just data flow
class Node {
public:
    Node();
    virtual ~Node() = default;
    
    // Main processing - called once per frame
    // dt: delta time in seconds
    virtual void update(float dt) {}
    
    // Optional Functional Hooks
    // draw() is only called if canDraw is set to true.
    virtual void draw() {}
    virtual void pullAudio(ofSoundBuffer& buffer, int index = 0) {}
    
    // Registration Hint
    // Set to true in the constructor if the node needs its draw() method called.
    bool canDraw = false;
    ofParameter<int> drawLayer;
    
    // Type-safe outputs - return nullptr if not supported
    // Derived classes override these to provide data
    // index: Which output to pull from (for nodes with multiple outputs)
    virtual ofTexture* getVideoOutput(int index = 0) { return nullptr; }
    
    // Human-readable name for UI display (override for richer info)
    virtual std::string getDisplayName() const { return name; }
    
    // Parameter reflection for GUI/DSL
    // ofParameterGroup enables automatic UI generation
    ofParameterGroup parameters;
    
    // Node metadata
    std::string name = "unnamed";
    std::string type = "Node";
    
    // Graph context (set by Graph when added)
    // Used by nodes to access other nodes for pull-based evaluation
    class Graph* graph = nullptr;
    int nodeId = -1;     // Stable ID that persists across add/remove operations
    
    // Static counter for generating unique nodeIds
    static std::atomic<int> nextNodeId;
    
public:
    // Internal state (public for Graph access, don't use directly)
    bool touched = false;  // Set during script execution to track active nodes
    
    // Serialization - each node controls its own format
    virtual ofJson serialize() const;
    virtual void deserialize(const ofJson& json);
    
    // Event notifications from Graph
    virtual void onInputConnected(int& toInput) {}
    virtual void onInputDisconnected(int& toInput) {}
    
    // Called when a parameter value changes (via script or UI)
    // Override in derived classes to react to parameter changes
    virtual void onParameterChanged(const std::string& paramName) {}
    
protected:

    // Helper to safely get a value from JSON with type-loose conversion
    // Handles strings-as-numbers ("1" -> 1.0) and other common JSON type mismatches
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
};
