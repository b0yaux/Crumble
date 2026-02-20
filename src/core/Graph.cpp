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
    
    // Notify the destination node that an input has been connected.
    // This allows nodes like VideoMixer to react immediately (e.g. expanding capacity)
    // so that subsequent parameter sets in a script (like opacity_N) target valid parameters.
    if (toNode >= 0 && toNode < (int)nodes.size()) {
        int dummy = toInput; 
        nodes[toNode]->onInputConnected(dummy);
    }
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

void Graph::removeInput(int toNode, int toInput) {
    // 1. Disconnect the specific input
    disconnect(toNode, toInput);
    
    // 2. Shift all higher-numbered inputs down by 1
    for (auto& conn : connections) {
        if (conn.toNode == toNode && conn.toInput > toInput) {
            conn.toInput--;
        }
    }
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
        validateTopology();
    }
    
    // Pull-based: update all sink nodes (nodes with no outgoing connections).
    // Each sink recursively pulls its inputs, and the lastUpdateFrame guard
    // prevents double-updates when sinks share upstream sources.
    for (int i = 0; i < (int)nodes.size(); i++) {
        if (getOutputConnections(i).empty()) {
            pullFromNode(i, dt);
        }
    }
}

ofTexture* Graph::getVideoOutput() {
    if (videoOutputNode >= 0 && videoOutputNode < nodes.size()) {
        return nodes[videoOutputNode]->getVideoOutput();
    }
    return nullptr;
}

ofSoundBuffer* Graph::getAudioOutput() {
    if (audioOutputNode >= 0 && audioOutputNode < nodes.size()) {
        return nodes[audioOutputNode]->getAudioOutput();
    }
    return nullptr;
}

void Graph::pullFromNode(int nodeIndex, float dt) {
    if (nodeIndex < 0 || nodeIndex >= (int)nodes.size()) return;
    
    // Check if already updated this frame
    auto& node = nodes[nodeIndex];
    if (!node) return; // Safety check
    
    uint64_t currentFrame = ofGetFrameNum();
    if (node->lastUpdateFrame == currentFrame) return;
    node->lastUpdateFrame = currentFrame;
    
    // Pull from all input connections first
    // Copy connections to avoid iterator invalidation if the update changes the graph
    auto inputs = getInputConnections(nodeIndex);
    for (const auto& conn : inputs) {
        pullFromNode(conn.fromNode, dt);
    }
    
    // Now update this node
    node->update(dt);
}

void Graph::validateTopology() {
    if (nodes.empty()) {
        executionDirty = false;
        return;
    }

    // Kahn's algorithm — only for cycle detection
    std::vector<int> inDegree(nodes.size(), 0);

    for (const auto& conn : connections) {
        if (conn.toNode >= 0 && conn.toNode < (int)nodes.size()) {
            inDegree[conn.toNode]++;
        }
    }

    std::vector<int> queue;
    for (int i = 0; i < (int)nodes.size(); i++) {
        if (inDegree[i] == 0) {
            queue.push_back(i);
        }
    }

    int visited = 0;
    while (!queue.empty()) {
        int current = queue.back();
        queue.pop_back();
        visited++;

        for (const auto& conn : connections) {
            if (conn.fromNode == current) {
                // Bounds check before decrementing
                if (conn.toNode >= 0 && conn.toNode < (int)inDegree.size()) {
                    inDegree[conn.toNode]--;
                    if (inDegree[conn.toNode] == 0) {
                        queue.push_back(conn.toNode);
                    }
                }
            }
        }
    }

    if (visited != (int)nodes.size()) {
        ofLogWarning("Graph") << "Cycle detected in graph, pull-based evaluation may loop";
    }

    executionDirty = false;
}

// Node Factory
void Graph::registerNodeType(const std::string& type, NodeCreator creator) {
    nodeTypes[type] = creator;
}

Node* Graph::createNode(const std::string& type, const std::string& name) {
    auto it = nodeTypes.find(type);
    if (it == nodeTypes.end()) {
        ofLogError("Graph") << "Unknown node type: " << type;
        return nullptr;
    }

    auto node = it->second();
    node->type = type;
    node->name = name.empty() ? type + "_" + std::to_string(nodes.size()) : name;
    node->nodeIndex = nodes.size();
    node->graph = this;
    
    Node* ptr = node.get();
    nodes.push_back(std::move(node));
    executionDirty = true;
    return ptr;
}

std::vector<std::string> Graph::getRegisteredTypes() const {
    std::vector<std::string> types;
    for (const auto& pair : nodeTypes) {
        types.push_back(pair.first);
    }
    return types;
}

// Serialization
ofJson Graph::serialize() const {
    // For a Graph, the "params" are the entire network structure
    return toJson();
}

void Graph::deserialize(const ofJson& json) {
    // Reconstruct the internal network from the json
    fromJson(json);
}

ofJson Graph::toJson() const {
    ofJson json;
    
    // Build nodes array first
    ofJson nodesJson = ofJson::array();
    for (const auto& node : nodes) {
        ofJson nodeJson;
        nodeJson["id"] = node->nodeIndex;
        nodeJson["type"] = node->type;
        nodeJson["name"] = node->name;

        // Each node serializes its own state. 
        // If it's a sub-graph, Node::serialize() will now call Graph::serialize() polymorphically.
        nodeJson["params"] = node->serialize();

        nodesJson.push_back(nodeJson);
    }
    
    // Build connections array
    ofJson connectionsJson = ofJson::array();
    for (const auto& conn : connections) {
        ofJson connJson;
        connJson["from"] = conn.fromNode;
        connJson["to"] = conn.toNode;
        connJson["fromOutput"] = conn.fromOutput;
        connJson["toInput"] = conn.toInput;
        connectionsJson.push_back(connJson);
    }
    
    // Build outputs object
    ofJson outputsJson;
    if (videoOutputNode >= 0) {
        outputsJson["video"] = videoOutputNode;
    }
    if (audioOutputNode >= 0) {
        outputsJson["audio"] = audioOutputNode;
    }
    
    // Assign in logical order: metadata -> nodes -> connections -> outputs
    json["version"] = "1.0";
    json["type"] = "Graph";
    json["nodes"] = nodesJson;
    json["connections"] = connectionsJson;
    json["outputs"] = outputsJson;
    
    // Also include the ofParameterGroup (masterOpacity, etc. if we add them to Graph)
    ofJson paramsJson;
    ofSerialize(paramsJson, parameters);
    json["internal_params"] = paramsJson;

    return json;
}

bool Graph::fromJson(const ofJson& json) {
    // Clear existing graph
    clear();

    // Check version
    if (json.contains("version")) {
        std::string version = json["version"];
        if (version != "1.0") {
            ofLogWarning("Graph") << "Loading patch with different version: " << version;
        }
    }
    
    // Load parameters
    if (json.contains("internal_params")) {
        ofDeserialize(json["internal_params"], parameters);
    }

    // Load nodes
    if (json.contains("nodes")) {
        for (const auto& nodeJson : json["nodes"]) {
            std::string type = nodeJson.value("type", "Node");
            std::string name = nodeJson.value("name", "");

            Node* node = createNode(type, name);
            if (!node) {
                ofLogError("Graph") << "Failed to create node of type: " << type;
                continue;
            }

            // Each node deserializes its own state
            ofJson params = nodeJson.contains("params") ? nodeJson["params"] : ofJson::object();
            node->deserialize(params);
        }
    }

            // Load connections (after all nodes created)
    if (json.contains("connections")) {
        for (const auto& connJson : json["connections"]) {
            int from = getSafeJson<int>(connJson, "from", -1);
            int to = getSafeJson<int>(connJson, "to", -1);
            int fromOutput = getSafeJson<int>(connJson, "fromOutput", 0);
            int toInput = getSafeJson<int>(connJson, "toInput", 0);

            if (from >= 0 && to >= 0 && from < (int)nodes.size() && to < (int)nodes.size()) {
                connect(from, to, fromOutput, toInput);
            }
        }
    }

    // Load outputs
    if (json.contains("outputs")) {
        const auto& outputs = json["outputs"];
        if (outputs.contains("video")) {
            videoOutputNode = getSafeJson<int>(outputs, "video", -1);
        }
        if (outputs.contains("audio")) {
            audioOutputNode = getSafeJson<int>(outputs, "audio", -1);
        }
    }

    executionDirty = true;
    
    return true;
}

bool Graph::saveToFile(const std::string& path) const {
    // Check if the path is empty
    if (path.empty()) {
        ofLogError("Graph") << "Cannot save to empty path";
        return false;
    }

    try {
        // Ensure parent directory exists
        std::string dir = ofFilePath::getEnclosingDirectory(ofToDataPath(path));
        if (!dir.empty() && !ofDirectory::doesDirectoryExist(dir)) {
            ofDirectory::createDirectory(dir, false, true);
        }
        
        ofJson json = toJson();
        std::string jsonStr = json.dump(2);
        return ofBufferToFile(path, ofBuffer(jsonStr.data(), jsonStr.size()));
    } catch (const std::exception& e) {
        ofLogError("Graph") << "Exception during save: " << e.what();
        return false;
    } catch (...) {
        ofLogError("Graph") << "Unknown exception during save";
        return false;
    }
}

bool Graph::loadFromFile(const std::string& path) {
    ofJson json = ofLoadJson(path);
    if (json.empty()) {
        ofLogError("Graph") << "Failed to load JSON from: " << path;
        return false;
    }
    return fromJson(json);
}
