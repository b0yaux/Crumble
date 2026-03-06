#include "Graph.h"
#include "Session.h"
#include "Transport.h"
#include "AssetRegistry.h"
#include "Config.h"
#include "../nodes/subgraph/Outlet.h"

// Static node type registry - shared by all Graph instances
std::map<std::string, Graph::NodeCreator> Graph::nodeTypes;

// Static script executor callback
Graph::ScriptExecutor Graph::s_scriptExecutor = nullptr;

Transport& Graph::getTransport() {
    return g_session->getTransport();
}

Graph::Graph() {
    type = "Graph";
    canDraw = true; // Graphs can contain drawable nodes
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
    Node* from = getNode(fromNode);
    if (to) {
        to->setInputNode(toInput, from);
        to->onInputConnected(toInput);
    }
    
    executionDirty = true;
    return true;
}

void Graph::disconnect(int toNode, int toInput) {
    Node* to = getNode(toNode);
    if (to) to->setInputNode(toInput, nullptr);

    connections.erase(
        std::remove_if(connections.begin(), connections.end(),
            [toNode, toInput](const Connection& c) {
                return c.toNode == toNode && c.toInput == toInput;
            }),
        connections.end()
    );
    executionDirty = true;
}

void Graph::compactInputIndices(int toNode, int removedInput) {
    for (auto& conn : connections) {
        if (conn.toNode == toNode && conn.toInput > removedInput) {
            conn.toInput--;
        }
    }
    executionDirty = true;
}

void Graph::removeNode(int nodeId) {
    // Check if node exists
    if (nodes.find(nodeId) == nodes.end()) return;
    
    std::unique_ptr<Node> nodeToDestroy;
    bool wasDrawable = false;

    {
        // Protect against audio thread access during mutation
        // We keep the lock as SHORT as possible
        std::lock_guard<std::recursive_mutex> lock(audioMutex);
        
        // Remove all connections to/from this node
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [nodeId](const Connection& c) {
                    return c.fromNode == nodeId || c.toNode == nodeId;
                }),
            connections.end()
        );
        
        auto it = nodes.find(nodeId);
        if (it != nodes.end()) {
            nodeToDestroy = std::move(it->second);
            wasDrawable = nodeToDestroy->canDraw;
            nodes.erase(it);
        }
        
        executionDirty = true;
    }
    
    // Destruction happens HERE, outside the lock!
    // If the node has large buffers (AudioFileSource), they are freed on the UI thread
    // without blocking the Audio Thread's pullAudio loop.
    nodeToDestroy.reset(); 

    if (wasDrawable) updateRenderList();
}

void Graph::clear() {
    std::unordered_map<int, std::unique_ptr<Node>> nodesToDestroy;
    {
        std::lock_guard<std::recursive_mutex> lock(audioMutex);
        nodesToDestroy = std::move(nodes);
        connections.clear();
        renderList.clear();
        executionDirty = true;
    }
    // nodesToDestroy goes out of scope here, deleting all nodes OUTSIDE the lock.
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
    
    // Push timing to all nodes for video/UI processing
    Context ctx;
    ctx.cycle = getTransport().cycle;
    ctx.cycleStep = 0; // No sub-sample stepping for 60fps frame updates
    ctx.frames = 1;    // One "frame" of logic
    ctx.dt = dt;
    
    for (int nodeId : traversalOrder) {
        auto it = nodes.find(nodeId);
        if (it != nodes.end() && it->second) {
            it->second->prepare(ctx);
            it->second->update(dt);
        }
    }
}

void Graph::draw() {
    for (Node* node : renderList) {
        node->draw();
    }
}

void Graph::updateRenderList() {
    renderList.clear();
    for (auto& [id, node] : nodes) {
        if (node && node->canDraw) {
            renderList.push_back(node.get());
        }
    }
    
    // Sort by drawLayer, then by nodeId for determinism
    std::sort(renderList.begin(), renderList.end(), [](Node* a, Node* b) {
        if (a->drawLayer != b->drawLayer) return a->drawLayer < b->drawLayer;
        return a->nodeId < b->nodeId;
    });
}

ofTexture* Graph::processVideo(int index) {
    // Search internal nodes for an 'Outlet' with matching index
    for (const auto& pair : nodes) {
        Node* node = pair.second.get();
        if (node && node->type == "Outlet") {
            // We need to cast to access outletIndex
            Outlet* outlet = static_cast<Outlet*>(node);
            if (outlet->outletIndex == index) {
                return node->getVideoOutput();
            }
        }
    }
    
    return nullptr;
}

void Graph::processAudio(ofSoundBuffer& buffer, int index) {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    
    // Push timing to all nodes before processing
    Context ctx;
    ctx.cycle = getTransport().cycle;
    ctx.cycleStep = getTransport().getCyclesPerSample(buffer.getSampleRate());
    ctx.frames = buffer.getNumFrames();
    
    for (auto& [id, node] : nodes) {
        node->prepare(ctx);
    }
    
    // Search internal nodes for an 'Outlet' with matching index
    for (const auto& pair : nodes) {
        Node* node = pair.second.get();
        if (node && node->type == "Outlet") {
            Outlet* outlet = static_cast<Outlet*>(node);
            if (outlet->outletIndex == index) {
                node->pullAudio(buffer);
                return;
            }
        }
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
    
    // Listen to drawLayer changes to re-sort the render list
    node->drawLayer.addListener(this, &Graph::onNodeLayerChanged);
    
    Node* ptr = node.get();
    
    {
        // Thread safety: Protect against audio thread access while modifying the nodes map
        std::lock_guard<std::recursive_mutex> lock(audioMutex);
        nodes[node->nodeId] = std::move(node);
    }
    
    executionDirty = true;
    if (ptr->canDraw) updateRenderList();
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
    
    updateRenderList();
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

std::string Graph::resolvePath(const std::string& path, const std::string& hint) const {
    if (path.empty()) return "";

    // 1. Try Logical Asset Registry
    std::string resolved = AssetRegistry::get().resolve(path, hint);
    if (!resolved.empty()) return resolved;

    // 2. Fallback to Search Paths / Data Path
    return ConfigManager::get().resolvePath(path);
}

void Graph::onScriptChanged(std::string& path) {
    if (!path.empty() && s_scriptExecutor) {
        ofLogNotice("Graph") << "Script parameter changed to: " << path << " for: " << name;
        // Execute the script directly into this graph. 
        // This graph acts as the container for the nodes created by the script.
        s_scriptExecutor(path, this);
    }
}

Graph* Graph::getParentGraph() const {
    // graph is already typed Graph*, so no cast needed
    return graph;
}

Node* Graph::getContainingNode() const {
    // Graph IS-A Node, so graph (which is Graph*) is always a valid Node*
    return graph;
}
