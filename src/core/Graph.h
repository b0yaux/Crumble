#pragma once
#include "Node.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <functional>
#include <string>
#include <mutex>

// Simple connection struct - no objects, just node IDs
// This is the key simplification: connections are just data
// Uses stable nodeId instead of volatile indices
struct Connection {
    int fromNode = -1;      // Node ID (stable across add/remove)
    int toNode = -1;        // Node ID (stable across add/remove)
    int fromOutput = 0;     // Which output (nodes can have multiple)
    int toInput = 0;        // Which input (nodes can have multiple)
};

// Graph IS a node - enables arbitrary nesting (TouchDesigner-style components)
// Graphs can contain other graphs, creating hierarchical structures
class Graph : public Node {
public:
    Graph();
    ~Graph() override;
    
    // Node management
    // Returns reference to created node for immediate configuration
    template<typename T, typename... Args>
    T& addNode(Args&&... args) {
        static_assert(std::is_base_of<Node, T>::value, "T must derive from Node");
        auto node = std::make_unique<T>(std::forward<Args>(args)...);
        node->nodeId = Node::nextNodeId.fetch_add(1);
        node->name = node->type + "_" + std::to_string(node->nodeId);
        node->graph = this;
        T& ref = *node;
        nodes[node->nodeId] = std::move(node);
        executionDirty = true;
        return ref;
    }
    
    // Connection management - just modifies the connections array
    // Returns true if connection was successful and didn't create a cycle
    bool connect(int fromNode, int toNode, int fromOutput = 0, int toInput = 0);
    void disconnect(int toNode, int toInput = 0);
    
    // Disconnect an input AND shift all higher-numbered inputs down by 1.
    // Used when removing a mixer layer: atomically closes the gap.
    void removeInput(int toNode, int toInput);
    
    void removeNode(int nodeId);
    void clear();
    
    // Get node by nodeId
    Node* getNode(int nodeId) {
        auto it = nodes.find(nodeId);
        if (it != nodes.end()) {
            return it->second.get();
        }
        return nullptr;
    }
    
    // Get connections for a specific node (by nodeId)
    std::vector<Connection> getInputConnections(int nodeId) const;
    std::vector<Connection> getOutputConnections(int nodeId) const;
    
    // Thread safety for audio thread
    std::mutex& getAudioMutex() { return audioMutex; }
    
    // Node interface implementation
    void update(float dt) override;
    void audioOut(ofSoundBuffer& buffer) override;
    
    // Access for serialization/debugging
    const std::unordered_map<int, std::unique_ptr<Node>>& getNodes() const { return nodes; }
    const std::vector<Connection>& getConnections() const { return connections; }
    
    // Graph state
    size_t getNodeCount() const { return nodes.size(); }
    size_t getConnectionCount() const { return connections.size(); }
    
    // Factory for creating nodes by type name (for deserialization)
    using NodeCreator = std::function<std::unique_ptr<Node>()>;
    void registerNodeType(const std::string& type, NodeCreator creator);
    Node* createNode(const std::string& type, const std::string& name = "");
    std::vector<std::string> getRegisteredTypes() const;
    
    // Serialization
    ofJson toJson() const;
    bool fromJson(const ofJson& json);
    
    // Override Node serialization for recursive nesting
    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;
    
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);
    
private:
    std::unordered_map<int, std::unique_ptr<Node>> nodes;
    std::vector<Connection> connections;

    // Topology validation flag
    bool executionDirty = true;
    
    std::mutex audioMutex;

    // Node type registry
    std::map<std::string, NodeCreator> nodeTypes;

    // Validate topology (cycle detection and sort) when graph changes
    // Returns true if graph is a valid DAG
    bool validateTopology();

    // The order in which nodes should be updated (Topological Sort)
    std::vector<int> traversalOrder;
};
