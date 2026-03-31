#pragma once
#include "Node.h"
#include "ofMain.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <functional>
#include <mutex>

// Forward declarations — full headers included in Graph.cpp
class AudioCache;
class Transport;

namespace crumble {
    struct ProcessorCommand;
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
    void onParameterChanged(const std::string& paramName) override;

    // --- Node Management ---
    Node* createNode(const std::string& type, const std::string& name = "");
    void removeNode(int nodeId);
    void clear();

    // Idempotent script reload — mirrors Session::beginScript/endScript
    // but scoped to this graph's children. Used by sub-graph hot-reload.
    // beginScript marks all children touched=false.
    // endScript removes untouched children and returns the count removed.
    void beginScript();
    int endScript();

    // Clear script-derived ephemeral state (outlets, proxies, Lua refs)
    // without destroying children. Called before idempotent sub-graph reload.
    void clearEphemeral();

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
    const std::vector<Connection>& getConnections() const { return connections; }

    // --- Navigation ---
    Graph* getParentGraph() const;
    Node* getContainingNode() const;

    // --- Routing overrides (sub-graph boundary resolution) ---
    Node* resolveInput(int toInput) override;
    Node* resolveOutput(int fromOutput) override;
    Node* resolveAudioOutput(int fromOutput) override;
    Node* resolveVideoOutput(int fromOutput) override;
    Node* resolveAudioInput(int toInput) override;
    Node* resolveVideoInput(int toInput) override;

    // --- Boundary declarations (inlet/outlet) ---
    void addOutlet(int nodeId, int index);
    void addInlet(int nodeId, int index);

    // --- Parameter proxy for sub-graphs (expose()) ---
    // When a sub-graph script calls expose("speed", childNode), this maps
    // "speed" on the Graph to childNode's "speed" parameter. The parent
    // sets graph.speed = 1.5 and the C++ interception layer forwards to
    // the child. Multiple children can share the same parent param name.
    struct ProxyTarget { int childId; std::string childParam; };
    void addProxyTarget(const std::string& parentParam, int childId, const std::string& childParam);
    std::vector<ProxyTarget> getProxyTargets(const std::string& parentParam) const;

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

    // Asset cache access — proxies to Session without exposing Session.h to node files
    AudioCache* getCache() const;
    int getSampleRate() const;
    
    void setTransport(Transport* t) { rootTransport = t; }
    Transport& getTransport();
    
    std::function<void()> onUpdate;
    std::function<void()> onClear;

    std::recursive_mutex& getAudioMutex() { return audioMutex; }

private:
    struct Boundary {
        int index = 0;
        Node* node = nullptr;
    };
    
    Transport* rootTransport = nullptr;
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

    // Inlet/outlet vectors — lightweight boundary declarations for sub-graph routing.
    std::vector<Boundary> outlets;
    std::vector<Boundary> inlets;

    // Parameter proxy map — maps a parent-facing param name to a list of
    // (childNodeId, childParam) pairs. Populated by expose() in sub-graph scripts.
    // Cleared on clear(), pruned on removeNode().
    std::unordered_map<std::string, std::vector<ProxyTarget>> proxyParams;
};
