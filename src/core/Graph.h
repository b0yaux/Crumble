#pragma once
#include "Node.h"
#include "ofMain.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <functional>
#include <mutex>

// Forward declaration — full header included in Graph.cpp
class Outlet;

namespace crumble {
    struct AudioCommand;
}

/**
 * A connection represents a link from one node's output to another node's input.
 */
struct Connection {
    int fromNode;
    int toNode;
    int fromOutput;
    int toInput;
    bool stale = false; // Added for pruning
};

/**
 * Graph is a container for Nodes and their connections. 
 */
class Graph : public Node {
public:
    using NodeCreator = std::function<std::unique_ptr<Node>()>;
    using ScriptExecutor = std::function<void(const std::string&, Graph*)>;

    Graph();
    virtual ~Graph();

    // --- Lifecycle ---
    void prepare(const Context& ctx) override;
    void update(float dt) override;
    void draw() override;
    void processAudio(ofSoundBuffer& buffer, int index = 0) override;

    // --- Node Management ---
    Node* createNode(const std::string& type, const std::string& name = "");
    void removeNode(int nodeId);
    void clear();
    void markConnectionsStale();
    void pruneStaleConnections();

    Node* getNode(int nodeId) {
        auto it = nodes.find(nodeId);
        if (it != nodes.end()) return it->second.get();
        return nullptr;
    }

    const std::unordered_map<int, std::unique_ptr<Node>>& getNodes() const { return nodes; }
    size_t getNodeCount() const { return nodes.size(); }

    // --- Topology & Connections ---
    bool connect(int fromNode, int toNode, int fromOutput = 0, int toInput = 0);
    void disconnect(int toNode, int toInput);
    void compactInputIndices(int toNode, int removedInput);

    std::vector<Connection> getInputConnections(int nodeId) const;
    std::vector<Connection> getOutputConnections(int nodeId) const;
    
    /**
     * Optimized access for the audio thread.
     */
    const std::vector<Connection>& getInputConnectionsRef(int nodeId) const;
    const std::vector<Connection>& getConnections() const { return connections; }

    // --- Navigation ---
    Graph* getParentGraph() const;
    Node* getContainingNode() const;

    // --- Factory Support ---
    static void registerNodeType(const std::string& type, NodeCreator creator);
    static void setScriptExecutor(ScriptExecutor executor) { s_scriptExecutor = executor; }
    std::vector<std::string> getRegisteredTypes() const;

    // --- Video Engine ---
    ofTexture* processVideo(int index = 0) override;

    // --- Serialization ---
    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;
    
    ofJson toJson() const;
    bool fromJson(const ofJson& json);
    
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);
    
    std::string resolvePath(const std::string& path, const std::string& hint = "") const override;
    
    std::recursive_mutex& getAudioMutex() { return audioMutex; }
    class Transport& getTransport();

private:
    std::unordered_map<int, std::unique_ptr<Node>> nodes;
    std::vector<Connection> connections;
    
    std::vector<Node*> renderList;
    void updateRenderList();
    void onNodeLayerChanged(int& layer);
    
    static std::map<std::string, NodeCreator> nodeTypes;
    static ScriptExecutor s_scriptExecutor;

    std::vector<int> traversalOrder; 
    bool validateTopology();         
    bool executionDirty = true;
    
    void onScriptChanged(std::string& path);
    ofParameter<std::string> scriptParam;
    
    mutable std::recursive_mutex audioMutex;

    // Connection caches for audio thread
    std::unordered_map<int, std::vector<Connection>> cachedInputs;
    std::unordered_map<int, std::vector<Connection>> cachedOutputs;
    void updateConnectionCache();

    // Outlet registry — keyed by nodeId, populated by createNode/removeNode/clear.
    // Eliminates the per-frame O(N) type-string scan in processAudio/processVideo.
    std::unordered_map<int, Outlet*> outletRegistry;
    void registerOutlet(Outlet* outlet);
    void unregisterOutlet(int nodeId);
};
