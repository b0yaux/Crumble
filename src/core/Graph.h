#pragma once
#include "Node.h"
#include "ofMain.h"
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <mutex>

/**
 * A connection represents a link from one node's output to another node's input.
 */
struct Connection {
    int fromNode;
    int toNode;
    int fromOutput;
    int toInput;
};

/**
 * Graph is a container for Nodes and their connections. 
 * Since Graph inherits from Node, it can be nested within other Graphs.
 */
class Graph : public Node {
public:
    using NodeCreator = std::function<std::unique_ptr<Node>()>;
    using ScriptExecutor = std::function<void(const std::string&, Graph*)>;

    Graph();
    virtual ~Graph();

    // --- Lifecycle ---
    void update(float dt) override;
    void draw() override;
    void processAudio(ofSoundBuffer& buffer, int index = 0) override;

    // --- Node Management ---
    Node* createNode(const std::string& type, const std::string& name = "");
    void removeNode(int nodeId);
    void clear();

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
    const std::vector<Connection>& getConnections() const { return connections; }

    // --- Navigation (for recursive lookup) ---
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
    
    // Internal JSON helpers
    ofJson toJson() const;
    bool fromJson(const ofJson& json);
    
    // File I/O
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);
    
    // Utilities
    std::string resolvePath(const std::string& path, const std::string& hint = "") const override;
    
    // Audio Mutex
    std::recursive_mutex& getAudioMutex() { return audioMutex; }
    
    // Helper to get global transport from Session
    class Transport& getTransport();

private:
    std::unordered_map<int, std::unique_ptr<Node>> nodes;
    std::vector<Connection> connections;
    
    // Node drawing order (sorted by layer)
    std::vector<Node*> renderList;
    void updateRenderList();
    void onNodeLayerChanged(int& layer) { updateRenderList(); }
    
    // Static registry for node instantiation
    static std::map<std::string, NodeCreator> nodeTypes;
    static ScriptExecutor s_scriptExecutor;

    // Execution state
    std::vector<int> traversalOrder; // Topological sort
    bool validateTopology();         // Re-sort and check for cycles
    bool executionDirty = true;
    
    void onScriptChanged(std::string& path);
    ofParameter<std::string> scriptParam;
    
    // Thread safety for audio thread vs main thread mutation
    mutable std::recursive_mutex audioMutex;
};
