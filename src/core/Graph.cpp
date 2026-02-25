#include "Graph.h"

// Static node type registry - shared by all Graph instances
std::map<std::string, Graph::NodeCreator> Graph::nodeTypes;

// Static script executor callback
Graph::ScriptExecutor Graph::s_scriptExecutor = nullptr;

Graph::Graph() {
    type = "Graph";
    parameters.add(scriptParam.set("script", ""));
    scriptParam.addListener(this, &Graph::onScriptChanged);
}

Graph::~Graph() {
    scriptParam.removeListener(this, &Graph::onScriptChanged);
    clear();
}

bool Graph::connect(int fromNode, int toNode, int fromOutput, int toInput) {
    if (fromNode == toNode) return false; // Self-loop
    
    // Check if these nodes exist
    if (nodes.find(fromNode) == nodes.end() || nodes.find(toNode) == nodes.end()) {
        return false;
    }

    // Disconnect existing input at that slot (single input per slot)
    disconnect(toNode, toInput);

    // Tentatively add the connection
    Connection conn;
    conn.fromNode = fromNode;
    conn.toNode = toNode;
    conn.fromOutput = fromOutput;
    conn.toInput = toInput;
    
    connections.push_back(conn);

    // Validate topology
    if (!validateTopology()) {
        // Cycle detected! Revert.
        connections.pop_back();
        ofLogError("Graph") << "Cycle detected when connecting " << fromNode << " -> " << toNode << ". Connection rejected.";
        return false;
    }

    // Success - notify nodes
    Node* to = getNode(toNode);
    if (to) to->onInputConnected(toInput);
    
    executionDirty = true;
    return true;
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

void Graph::removeNode(int nodeId) {
    // Check if node exists
    if (nodes.find(nodeId) == nodes.end()) return;
    
    // Protect against audio thread access during mutation
    std::lock_guard<std::mutex> lock(audioMutex);
    
    // Remove all connections to/from this node
    connections.erase(
        std::remove_if(connections.begin(), connections.end(),
            [nodeId](const Connection& c) {
                return c.fromNode == nodeId || c.toNode == nodeId;
            }),
        connections.end()
    );
    
    // Remove node - no index update needed since we use stable nodeId
    nodes.erase(nodeId);
    
    executionDirty = true;
}

void Graph::clear() {
    std::unordered_map<int, std::unique_ptr<Node>> nodesToDestroy;
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        nodesToDestroy = std::move(nodes);
        connections.clear();
        executionDirty = true;
    }
}
std::vector<Connection> Graph::getInputConnections(int nodeId) const {
    std::vector<Connection> result;
    for (const auto& conn : connections) {
        if (conn.toNode == nodeId) {
            result.push_back(conn);
        }
    }
    return result;
}

std::vector<Connection> Graph::getOutputConnections(int nodeId) const {
    std::vector<Connection> result;
    for (const auto& conn : connections) {
        if (conn.fromNode == nodeId) {
            result.push_back(conn);
        }
    }
    return result;
}

void Graph::update(float dt) {
    if (executionDirty) {
        validateTopology();
    }
    
    // Iterative evaluation using pre-computed topological order
    for (int nodeId : traversalOrder) {
        auto it = nodes.find(nodeId);
        if (it != nodes.end() && it->second) {
            it->second->update(dt);
        }
    }
    
    // Propagate update to child graph if present
    if (childGraph) {
        childGraph->update(dt);
    }
}

ofTexture* Graph::getVideoOutput() {
    // If has childGraph (deeper nesting), route to it
    if (childGraph) {
        return childGraph->getVideoOutput();
    }
    
    // If this IS a subgraph (has nodes like Inlet/Outlet), find Outlet
    if (!nodes.empty()) {
        for (const auto& pair : nodes) {
            Node* node = pair.second.get();
            if (node && node->type == "Outlet") {
                return node->getVideoOutput();
            }
        }
    }
    
    return nullptr;
}

void Graph::audioOut(ofSoundBuffer& buffer) {
    std::lock_guard<std::mutex> lock(audioMutex);
    
    // Route audio through child graph if present
    if (childGraph) {
        childGraph->audioOut(buffer);
        return;
    }
    
    buffer.set(0);
}

bool Graph::validateTopology() {
    traversalOrder.clear();
    if (nodes.empty()) {
        executionDirty = false;
        return true;
    }

    // Kahn's algorithm for Topological Sort & Cycle Detection
    std::vector<int> nodeIdList;
    std::unordered_map<int, int> nodeIdToIndex;
    int idx = 0;
    for (const auto& pair : nodes) {
        nodeIdToIndex[pair.first] = idx;
        nodeIdList.push_back(pair.first);
        idx++;
    }
    
    std::vector<int> inDegree(nodes.size(), 0);
    for (const auto& conn : connections) {
        auto toIt = nodeIdToIndex.find(conn.toNode);
        if (toIt != nodeIdToIndex.end()) {
            inDegree[toIt->second]++;
        }
    }

    // Use a queue for BFS-based topological sort
    std::vector<int> queue;
    for (int i = 0; i < (int)nodeIdList.size(); i++) {
        if (inDegree[i] == 0) {
            queue.push_back(nodeIdList[i]);
        }
    }

    // Process nodes
    size_t visitedCount = 0;
    while (!queue.empty()) {
        int currentNodeId = queue.front();
        queue.erase(queue.begin());
        
        traversalOrder.push_back(currentNodeId);
        visitedCount++;

        for (const auto& conn : connections) {
            if (conn.fromNode == currentNodeId) {
                auto toIt = nodeIdToIndex.find(conn.toNode);
                if (toIt != nodeIdToIndex.end()) {
                    int toIdx = toIt->second;
                    inDegree[toIdx]--;
                    if (inDegree[toIdx] == 0) {
                        queue.push_back(conn.toNode);
                    }
                }
            }
        }
    }

    executionDirty = false;
    
    // If we couldn't visit all nodes, there's a cycle
    return (visitedCount == nodes.size());
}

// Node Factory
void Graph::registerNodeType(const std::string& type, NodeCreator creator) {
    Graph::nodeTypes[type] = creator;
}

Node* Graph::createNode(const std::string& type, const std::string& name) {
    auto it = Graph::nodeTypes.find(type);
    if (it == Graph::nodeTypes.end()) {
        ofLogError("Graph") << "Unknown node type: " << type;
        return nullptr;
    }

    auto node = it->second();
    node->type = type;
    node->name = name.empty() ? type + "_" + std::to_string(nodes.size()) : name;
    node->nodeId = Node::nextNodeId.fetch_add(1);
    node->graph = this;
    
    Node* ptr = node.get();
    nodes[node->nodeId] = std::move(node);
    executionDirty = true;
    return ptr;
}

std::vector<std::string> Graph::getRegisteredTypes() const {
    std::vector<std::string> types;
    for (const auto& pair : Graph::nodeTypes) {
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
    for (const auto& [id, node] : nodes) {
        ofJson nodeJson;
        nodeJson["id"] = node->nodeId;
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
    
    // Assign in logical order: metadata -> nodes -> connections -> outputs
    json["version"] = "1.0";
    json["type"] = "Graph";
    json["nodes"] = nodesJson;
    json["connections"] = connectionsJson;
    json["outputs"] = outputsJson;
    
    // Serialize child graph if present
    if (childGraph) {
        json["childGraph"] = childGraph->toJson();
    }
    
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
            int nodeId = getSafeJson<int>(nodeJson, "id", -1);

            // Get the node creator
            auto it = Graph::nodeTypes.find(type);
            if (it == Graph::nodeTypes.end()) {
                ofLogError("Graph") << "Unknown node type: " << type;
                continue;
            }

            // Create node
            auto node = it->second();
            node->type = type;
            node->name = name.empty() ? type + "_" + std::to_string(nodes.size()) : name;
            node->graph = this;
            
            // Preserve nodeId from JSON if provided, otherwise generate new one
            if (nodeId >= 0) {
                node->nodeId = nodeId;
                // Update nextNodeId to be beyond any loaded IDs
                if (nodeId >= Node::nextNodeId.load()) {
                    Node::nextNodeId.store(nodeId + 1);
                }
            } else {
                node->nodeId = Node::nextNodeId.fetch_add(1);
            }

            // Each node deserializes its own state
            ofJson params = nodeJson.contains("params") ? nodeJson["params"] : ofJson::object();
            node->deserialize(params);

            // Insert into map with the nodeId as key
            nodes[node->nodeId] = std::move(node);
        }
    }

            // Load connections (after all nodes created)
    if (json.contains("connections")) {
        for (const auto& connJson : json["connections"]) {
            int from = getSafeJson<int>(connJson, "from", -1);
            int to = getSafeJson<int>(connJson, "to", -1);
            int fromOutput = getSafeJson<int>(connJson, "fromOutput", 0);
            int toInput = getSafeJson<int>(connJson, "toInput", 0);

            // Use nodeId-based lookup instead of array index
            if (from >= 0 && to >= 0 && nodes.find(from) != nodes.end() && nodes.find(to) != nodes.end()) {
                connect(from, to, fromOutput, toInput);
            }
        }
    }

    // Load outputs
    if (json.contains("outputs")) {
        const auto& outputs = json["outputs"];
    }

    // Load child graph if present
    if (json.contains("childGraph")) {
        getOrCreateChildGraph()->fromJson(json["childGraph"]);
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

void Graph::onScriptChanged(std::string& path) {
    if (!path.empty() && s_scriptExecutor) {
        ofLogNotice("Graph") << "Script parameter changed to: " << path << " for: " << name;
        s_scriptExecutor(path, getOrCreateChildGraph());
    }
}

Graph* Graph::getOrCreateChildGraph() {
    if (!childGraph) {
        childGraph = std::make_unique<Graph>();
        childGraph->graph = this;
        childGraph->type = "Graph";
        childGraph->name = this->name + "_child";
        
        ofLogNotice("Graph") << "Created child graph for: " << name;
    }
    return childGraph.get();
}

Graph* Graph::getParentGraph() const {
    if (graph && dynamic_cast<Graph*>(graph)) {
        return static_cast<Graph*>(graph);
    }
    return nullptr;
}

Node* Graph::getContainingNode() const {
    if (graph && dynamic_cast<Node*>(graph)) {
        return static_cast<Node*>(graph);
    }
    return nullptr;
}
