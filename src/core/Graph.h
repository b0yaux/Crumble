#pragma once
#include "Node.h"
#include <vector>
#include <memory>
#include <algorithm>

// Simple connection struct - no objects, just indices
// This is the key simplification: connections are just data
struct Connection {
    int fromNode = -1;      // Index in nodes array
    int toNode = -1;        // Index in nodes array
    int fromOutput = 0;     // Which output (nodes can have multiple)
    int toInput = 0;        // Which input (nodes can have multiple)
};

// Graph IS a node - enables arbitrary nesting (TouchDesigner-style components)
// Graphs can contain other graphs, creating hierarchical structures
class Graph : public Node {
public:
    Graph();
    virtual ~Graph() = default;
    
    // Node management
    // Returns reference to created node for immediate configuration
    template<typename T, typename... Args>
    T& addNode(Args&&... args) {
        static_assert(std::is_base_of<Node, T>::value, "T must derive from Node");
        auto node = std::make_unique<T>(std::forward<Args>(args)...);
        node->name = type + "_" + std::to_string(nodes.size());
        node->nodeIndex = nodes.size();
        node->graph = this;
        T& ref = *node;
        nodes.push_back(std::move(node));
        executionDirty = true;
        return ref;
    }
    
    // Direct node pointer addition (for moving existing nodes)
    void addNode(std::unique_ptr<Node> node);
    
    // Connection management - just modifies the connections array
    void connect(int fromNode, int toNode, int fromOutput = 0, int toInput = 0);
    void disconnect(int toNode, int toInput = 0);
    void removeNode(int nodeIndex);
    void clear();
    
    // Get node by index
    Node* getNode(int index) {
        if (index >= 0 && index < nodes.size()) {
            return nodes[index].get();
        }
        return nullptr;
    }
    
    // Get connections for a specific node
    std::vector<Connection> getInputConnections(int nodeIndex) const;
    std::vector<Connection> getOutputConnections(int nodeIndex) const;
    
    // Set which nodes provide the graph's outputs
    void setVideoOutputNode(int nodeIndex);
    void setAudioOutputNode(int nodeIndex);
    int getVideoOutputNode() const { return videoOutputNode; }
    int getAudioOutputNode() const { return audioOutputNode; }
    
    // Node interface implementation
    void update(float dt) override;
    ofTexture* getVideoOutput() override;
    ofSoundBuffer* getAudioOutput() override;
    
    // Access for serialization/debugging
    const std::vector<std::unique_ptr<Node>>& getNodes() const { return nodes; }
    const std::vector<Connection>& getConnections() const { return connections; }
    
    // Graph state
    size_t getNodeCount() const { return nodes.size(); }
    size_t getConnectionCount() const { return connections.size(); }
    
private:
    std::vector<std::unique_ptr<Node>> nodes;
    std::vector<Connection> connections;
    
    // Execution order (cached topological sort)
    std::vector<int> executionOrder;
    bool executionDirty = true;
    
    // Output nodes (indices into nodes array)
    int videoOutputNode = -1;
    int audioOutputNode = -1;
    
    // Build execution order using topological sort
    void rebuildExecutionOrder();
    
    // Pull-based evaluation helpers
    void pullVideoFromNode(int nodeIndex);
    void pullAudioFromNode(int nodeIndex);
};
