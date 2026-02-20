# UI Decoupling & Serialization Architecture Research

## Executive Summary

Your PatchLoader approach failed because it tried to be both a JSON parser AND a graph builder simultaneously, creating tight coupling. The solution is to separate concerns into distinct layers: **Graph Model** (core), **Serializer** (I/O), and **Command Layer** (interaction).

---

## What Went Wrong with PatchLoader

### 1. **Wrong Abstraction Level**
```cpp
// PatchLoader tried to do too much:
class PatchLoader {
    Graph* graph;  // ❌ Direct graph access violates encapsulation
    NodeFactory nodeFactory;  // ❌ Creates nodes directly
    GraphBuilder graphBuilder;  // ❌ Callback complexity
};
```
**Problem**: PatchLoader was both deserializing JSON AND mutating the graph. This violates Single Responsibility Principle.

### 2. **Callback Hell**
```cpp
using NodeFactory = std::function<std::unique_ptr<Node>(...)>;
using GraphBuilder = std::function<void(const PatchConfig&)>;
```
**Problem**: Callbacks add indirection without benefit. You can't debug them easily, and they obscure the flow.

### 3. **Unused Graph Pointer**
The `Graph* graph` member was stored but never used because the callbacks handled everything. This indicates the design had the wrong separation.

### 4. **No Clear Contract**
PatchLoader tried to handle both structure (nodes/edges) AND parameters (opacity/blend). These should be separate concerns.

---

## Recommended Architecture: 3-Layer Design

```
┌─────────────────────────────────────────┐
│  Layer 3: UI Controllers                │
│  - ImGui panels                         │
│ - OSC/MIDI listeners                   │
│ - Keyboard shortcuts                    │
│ - Lua/Python scripting                  │
└──────────────┬──────────────────────────┘
               │ Commands (generic)
               ▼
┌─────────────────────────────────────────┐
│  Layer 2: Command Queue                 │
│  - AddNodeCmd                           │
│ - ConnectCmd                            │
│ - SetParamCmd                           │
│ - LoadPatchCmd                          │
└──────────────┬──────────────────────────┘
               │ Executes on
               ▼
┌─────────────────────────────────────────┐
│  Layer 1: Graph Model + Serializer      │
│  - Graph (owns nodes)                   │
│ - Node hierarchy                        │
│ - Connections                           │
│ - toJson()/fromJson()                   │
└─────────────────────────────────────────┘
```

---

## Layer 1: Graph Model (Core)

### Keep your existing Graph/Node - it works!

```cpp
// Add serialization methods to Graph:
class Graph {
public:
    ofJson toJson() const;
    bool fromJson(const ofJson& json);  // Returns false on error
    
    // Factory for creating typed nodes
    template<typename T>
    T* createNode(const std::string& name = "");
    Node* createNode(const std::string& type, const std::string& name);
};
```

### Why use ofJson (nlohmann::json)?

✅ **Ships with openFrameworks** - no extra dependencies  
✅ **Automatic ofParameter serialization** via `ofSerialize()`/`ofDeserialize()`  
✅ **Standard** - familiar to all OF developers  
✅ **Hierarchical** - handles nested structures naturally  

---

## Layer 2: Command Pattern

### Commands enable UI decoupling

```cpp
// Base command - everything flows through this
struct Command {
    virtual ~Command() = default;
    virtual void execute(Graph& graph) = 0;
    virtual void undo(Graph& graph) { /* optional */ }
};

// Concrete commands
struct AddNodeCmd : Command {
    std::string type;
    std::string name;
    int createdIndex = -1;
    
    void execute(Graph& graph) override {
        Node* node = graph.createNode(type, name);
        createdIndex = node->nodeIndex;
    }
};

struct SetParamCmd : Command {
    int nodeIndex;
    std::string paramName;
    float value;
    float oldValue;
    
    void execute(Graph& graph) override {
        // Set parameter on node
    }
    void undo(Graph& graph) override {
        // Restore old value
    }
};
```

### Benefits:
- **UI Agnostic**: Any interface (ImGui, OSC, scripting) creates commands
- **Undo/Redo**: Built-in support for free
- **Logging**: Record every change for debugging
- **Network**: Send commands over OSC for remote control
- **Testable**: Commands are easy to unit test

---

## Layer 3: Multiple UI Frontends

With the command layer, you can have multiple interaction methods:

### A) ImGui (Recommended for visual editing)
```cpp
// Use imgui-node-editor or imnodes addons
// They provide node graph visualization
// You render nodes, they handle interaction
```

### B) JSON Hot-Reload (File-based)
```cpp
class JsonHotReloader {
    std::string watchPath;
    uint64_t lastModified;
    
    void update() {
        if (fileModified(watchPath)) {
            ofJson json = ofLoadJson(watchPath);
            // Create commands from JSON diff
            // Apply to graph
        }
    }
};
```

### C) OSC Remote Control
```cpp
// Receive commands from TouchDesigner, Max/MSP, etc.
void onOscMessage(ofxOscMessage& msg) {
    if (msg.getAddress() == "/layer/opacity") {
        cmdQueue.push(std::make_unique<SetParamCmd>(
            msg.getArgAsInt(0),  // layer
            "opacity", 
            msg.getArgAsFloat(1)  // value
        ));
    }
}
```

### D) Lua/Python Scripting
```cpp
// Embed a scripting language
// Users write scripts that create commands
```

---

## Implementation Roadmap

### Phase 1: Graph Serialization (Foundation)
1. Add `toJson()` and `fromJson()` to Graph class
2. Add type registry for node factory
3. Test save/load round-trip
4. Remove broken PatchLoader

### Phase 2: Command System
1. Create Command base class
2. Implement AddNodeCmd, RemoveNodeCmd, ConnectCmd
3. Create CommandQueue that executes on main thread
4. Wire commands into ofApp key handlers

### Phase 3: Parameter Hot-Reload
1. Add file watcher for JSON
2. Compare current state vs file on modification
3. Generate minimal SetParamCmds for differences
4. This enables "tweak in editor, see in real-time"

### Phase 4: ImGui Integration
1. Add ofxImGui to project
2. Create basic parameter panels
3. (Optional) Add node graph visualization via imgui-node-editor

---

## JSON Schema Recommendation

```json
{
  "version": "1.0",
  "graph": {
    "nodes": [
      {
        "id": 0,
        "type": "VideoMixer",
        "name": "MainMixer",
        "params": {
          "numLayers": 4,
          "layerOpacity": [1.0, 0.5, 0.8, 1.0]
        }
      },
      {
        "id": 1,
        "type": "VideoFileSource",
        "name": "Video1",
        "params": {
          "file": "video1.mov",
          "loop": true
        }
      }
    ],
    "connections": [
      {"from": 1, "to": 0, "fromOutput": 0, "toInput": 0}
    ],
    "output": {
      "video": 0
    }
  }
}
```

---

## Key Insights from Research

### 1. **TouchDesigner-style Systems Use Commands**
All professional node systems (TouchDesigner, Houdini, Nuke) use command-based architectures for undo/redo and UI decoupling.

### 2. **ofParameter Already Solves 80% of Serialization**
```cpp
ofParameterGroup parameters;  // ← This is all you need
ofJson json;
ofSerialize(json, parameters);  // ← Automatic serialization
ofDeserialize(json, parameters);  // ← Automatic deserialization
```

### 3. **Don't Build a GUI Framework**
Focus on the data model and serialization. Use existing addons:
- **ofxImGui**: Parameter panels
- **imgui-node-editor**: Node graph visualization
- **ofxOsc**: Network control

### 4. **Hot-Reload is Different from Full Serialization**
- **Full serialization**: Save/load entire graph structure
- **Hot-reload**: Update parameters without rebuilding graph
- These are separate features with different use cases

---

## Why This Approach Works

| Your Old Approach | Recommended Approach |
|------------------|---------------------|
| PatchLoader does everything | Clean separation of concerns |
| Callback complexity | Simple Command objects |
| Tight coupling | UI-agnostic core |
| Hard to test | Commands are unit testable |
| Single interaction method | Multiple frontends possible |
| No undo support | Built-in undo/redo |

---

## Next Steps

**Start with Phase 1**: Add JSON serialization to Graph. This:
- Removes broken PatchLoader
- Gives you immediate save/load capability
- Provides foundation for everything else
- Takes ~2-3 hours to implement

**Then Phase 2**: Add Command system. This:
- Decouples UI from graph
- Enables undo/redo
- Prepares for multiple interaction methods
- Takes ~4-6 hours to implement

**Later Phases**: ImGui, hot-reload, OSC - these build on the foundation and can be added incrementally.

---

## References

- **Command Pattern**: Gang of Four Design Patterns
- **ofJson/nlohmann::json**: https://json.nlohmann.me/
- **ofParameter serialization**: openFrameworks documentation
- **imgui-node-editor**: https://github.com/thedmd/imgui-node-editor
- **JSON Graph Format**: https://github.com/jsongraph/json-graph-specification
