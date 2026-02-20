#pragma once
#include "ofMain.h"

// Minimal base class for all nodes in the graph
// No Port objects, no ConnectionManager - just data flow
class Node {
public:
    virtual ~Node() = default;
    
    // Main processing - called once per frame
    // dt: delta time in seconds
    virtual void update(float dt) {}
    
    // Mark node as needing recalculation (currently unused — reserved for future invalidation propagation)
    void invalidate() { dirty = true; }
    
    // Type-safe outputs - return nullptr if not supported
    // Derived classes override these to provide data
    virtual ofTexture* getVideoOutput() { return nullptr; }
    virtual ofSoundBuffer* getAudioOutput() { return nullptr; }
    
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
    int nodeIndex = -1;
    
public:
    // Internal state (public for Graph access, don't use directly)
    bool dirty = true;  // True if cached output needs recalculation
    uint64_t lastUpdateFrame = UINT64_MAX;  // For pull-based caching (UINT64_MAX = never updated)
    
    // Serialization - each node controls its own format
    virtual ofJson serialize() const;
    virtual void deserialize(const ofJson& json);
    
    // Called after deserialization completes
    virtual void deserializeComplete() {}

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
