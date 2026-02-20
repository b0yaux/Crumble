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
};
