#include "Graph.h"

Graph::Graph() {
    type = "Graph";
}

void Graph::addNode(std::unique_ptr<Node> node) {
    node->nodeIndex = nodes.size();
    node->graph = this;
    nodes.push_back(std::move(node));
    executionDirty = true;
}

void Graph::connect(int fromNode, int toNode, int fromOutput, int toInput) {
    // Remove existing connection to this input
    disconnect(toNode, toInput);
    
    // Add new connection
    connections.push_back({fromNode, toNode, fromOutput, toInput});
    executionDirty = true;
}

void Graph::disconnect(int toNode, int toInput) {
    connections.erase(
        std::remove_if(connections.begin(), connections.end(),
            [toNode, toInput](const Connection& c) {
                return c.toNode == toNode && c.toInput == toInput;
            }),
        connections.end()
    );
    executionDirty = true;
}

void Graph::removeNode(int nodeIndex) {
    if (nodeIndex < 0 || nodeIndex >= nodes.size()) return;
    
    // Remove all connections to/from this node
    connections.erase(
        std::remove_if(connections.begin(), connections.end(),
            [nodeIndex](const Connection& c) {
                return c.fromNode == nodeIndex || c.toNode == nodeIndex;
            }),
        connections.end()
    );
    
    // Remove node
    nodes.erase(nodes.begin() + nodeIndex);
    
    // Update indices for remaining nodes and connections
    for (int i = nodeIndex; i < nodes.size(); i++) {
        nodes[i]->nodeIndex = i;
    }
    
    for (auto& conn : connections) {
        if (conn.fromNode > nodeIndex) conn.fromNode--;
        if (conn.toNode > nodeIndex) conn.toNode--;
    }
    
    // Update output nodes
    if (videoOutputNode == nodeIndex) {
        videoOutputNode = -1;
    } else if (videoOutputNode > nodeIndex) {
        videoOutputNode--;
    }
    
    if (audioOutputNode == nodeIndex) {
        audioOutputNode = -1;
    } else if (audioOutputNode > nodeIndex) {
        audioOutputNode--;
    }
    
    executionDirty = true;
}

void Graph::clear() {
    nodes.clear();
    connections.clear();
    executionOrder.clear();
    videoOutputNode = -1;
    audioOutputNode = -1;
    executionDirty = true;
}

std::vector<Connection> Graph::getInputConnections(int nodeIndex) const {
    std::vector<Connection> result;
    for (const auto& conn : connections) {
        if (conn.toNode == nodeIndex) {
            result.push_back(conn);
        }
    }
    return result;
}

std::vector<Connection> Graph::getOutputConnections(int nodeIndex) const {
    std::vector<Connection> result;
    for (const auto& conn : connections) {
        if (conn.fromNode == nodeIndex) {
            result.push_back(conn);
        }
    }
    return result;
}

void Graph::setVideoOutputNode(int nodeIndex) {
    videoOutputNode = nodeIndex;
}

void Graph::setAudioOutputNode(int nodeIndex) {
    audioOutputNode = nodeIndex;
}

void Graph::update(float dt) {
    if (executionDirty) {
        rebuildExecutionOrder();
    }
    
    // Update all nodes in dependency order
    for (int idx : executionOrder) {
        if (idx >= 0 && idx < nodes.size()) {
            nodes[idx]->update(dt);
        }
    }
}

ofTexture* Graph::getVideoOutput() {
    if (videoOutputNode >= 0 && videoOutputNode < nodes.size()) {
        // Pull-based: this will recursively update inputs
        pullVideoFromNode(videoOutputNode);
        return nodes[videoOutputNode]->getVideoOutput();
    }
    return nullptr;
}

ofSoundBuffer* Graph::getAudioOutput() {
    if (audioOutputNode >= 0 && audioOutputNode < nodes.size()) {
        pullAudioFromNode(audioOutputNode);
        return nodes[audioOutputNode]->getAudioOutput();
    }
    return nullptr;
}

void Graph::pullVideoFromNode(int nodeIndex) {
    if (nodeIndex < 0 || nodeIndex >= nodes.size()) return;
    
    // Check if already updated this frame
    auto& node = nodes[nodeIndex];
    uint64_t currentFrame = ofGetFrameNum();
    if (node->lastUpdateFrame == currentFrame) return;
    node->lastUpdateFrame = currentFrame;
    
    // Pull from all input connections first
    auto inputs = getInputConnections(nodeIndex);
    for (const auto& conn : inputs) {
        // Recursively pull from source
        pullVideoFromNode(conn.fromNode);
    }
    
    // Now update this node
    node->update(ofGetLastFrameTime());
}

void Graph::pullAudioFromNode(int nodeIndex) {
    if (nodeIndex < 0 || nodeIndex >= nodes.size()) return;
    
    auto& node = nodes[nodeIndex];
    uint64_t currentFrame = ofGetFrameNum();
    if (node->lastUpdateFrame == currentFrame) return;
    node->lastUpdateFrame = currentFrame;
    
    auto inputs = getInputConnections(nodeIndex);
    for (const auto& conn : inputs) {
        pullAudioFromNode(conn.fromNode);
    }
    
    node->update(ofGetLastFrameTime());
}

void Graph::rebuildExecutionOrder() {
    executionOrder.clear();
    
    if (nodes.empty()) {
        executionDirty = false;
        return;
    }
    
    // Kahn's algorithm for topological sort
    std::vector<int> inDegree(nodes.size(), 0);
    
    // Calculate in-degrees
    for (const auto& conn : connections) {
        if (conn.toNode >= 0 && conn.toNode < nodes.size()) {
            inDegree[conn.toNode]++;
        }
    }
    
    // Start with nodes that have no inputs
    std::vector<int> queue;
    for (int i = 0; i < nodes.size(); i++) {
        if (inDegree[i] == 0) {
            queue.push_back(i);
        }
    }
    
    // Process queue
    while (!queue.empty()) {
        int current = queue.back();
        queue.pop_back();
        executionOrder.push_back(current);
        
        // Find all nodes that depend on current
        for (const auto& conn : connections) {
            if (conn.fromNode == current) {
                inDegree[conn.toNode]--;
                if (inDegree[conn.toNode] == 0) {
                    queue.push_back(conn.toNode);
                }
            }
        }
    }
    
    // Check for cycles
    if (executionOrder.size() != nodes.size()) {
        ofLogWarning("Graph") << "Cycle detected in graph, execution order may be incorrect";
    }
    
    executionDirty = false;
}
