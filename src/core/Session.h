#pragma once
#include "Graph.h"
#include "AssetPool.h"
#include <vector>

// Global session pointer for nodes that need asset access
extern class Session* g_session;

// Session — the live working context for a Crumble session.
// Owns the node graph and provides the primary API surface for any
// interaction layer (keyboard, scripting, OSC, GUI).
//
// Graph primitives (addNode, connect, etc.) are exposed directly.
// Undo is checkpoint-based: snapshot the entire graph state as JSON
// before significant operations, restore on undo. Simple and correct
// for graphs of any realistic size (<100 nodes).

class Session {
public:
    Session() { g_session = this; }
    ~Session() { g_session = nullptr; }

    // --- Asset Management ---
    AssetPool& getAssets() { return assetPool; }

    // --- Graph primitives (the primary API) ---

    Node* addNode(const std::string& type, const std::string& name = "");
    void  removeNode(int index);
    void  connect(int fromNode, int toNode, int fromOutput = 0, int toInput = 0);
    void  disconnect(int toNode, int toInput = 0);
    void  removeInput(int toNode, int toInput);
    void  clear();
    
    // --- Script lifecycle (for idempotent reloading) ---
    
    void beginScript();   // Clear touched flags on all nodes
    void endScript();     // Remove nodes not touched during script
    void touchNode(int index);  // Mark node as active

    // --- Lifecycle ---

    void update(float dt);

    // --- Node access ---

    Node* getNode(int index);
    Node* getNode(const std::string& name);
    int   getNodeCount() const;

    // --- Search & Inspection ---

    Node* findNodeByName(const std::string& name);
    
    template<typename T>
    T* findFirstNode() {
        for (const auto& node : graph.getNodes()) {
            T* ptr = dynamic_cast<T*>(node.get());
            if (ptr) return ptr;
        }
        return nullptr;
    }

    template<typename T>
    std::vector<T*> findAllNodes() {
        std::vector<T*> result;
        for (const auto& node : graph.getNodes()) {
            T* ptr = dynamic_cast<T*>(node.get());
            if (ptr) result.push_back(ptr);
        }
        return result;
    }

    template<typename T>
    T* getNodeAs(int index) {
        return dynamic_cast<T*>(getNode(index));
    }

    // --- Undo / Redo (checkpoint-based) ---

    void checkpoint();      // Snapshot current graph state
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    // --- Persistence ---

    bool save(const std::string& path);
    bool load(const std::string& path);

    // --- Factory ---

    void registerNodeType(const std::string& type, Graph::NodeCreator creator);
    std::vector<std::string> getRegisteredTypes() const;

    // --- Direct graph access (for nodes that need graph context) ---

    Graph&       getGraph();
    const Graph& getGraph() const;

private:
    Graph graph;
    AssetPool assetPool;

    // Checkpoint-based undo history
    std::vector<ofJson> snapshots;
    int snapshotPos = -1;
    static constexpr size_t maxSnapshots = 100;
};
