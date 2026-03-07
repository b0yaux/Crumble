#include "Graph.h"
#include "Session.h"
#include "Transport.h"
#include "AssetRegistry.h"
#include "Config.h"
#include "AudioCommand.h"
#include "NodeProcessor.h"
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
    parameters->add(scriptParam.set("script", ""));
    scriptParam.addListener(this, &Graph::onScriptChanged);
}

Graph::~Graph() {
    scriptParam.removeListener(this, &Graph::onScriptChanged);
    clear();
}

void Graph::prepare(const Context& ctx) {
    // 1. Prepare self (modulators on the graph node itself)
    Node::prepare(ctx);
    
    // 2. Recursive Prepare: Prepare all internal nodes.
    // This ensures all patterns in the graph are calculated exactly once.
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    for (auto& [id, node] : nodes) {
        if (node) node->prepare(ctx);
    }
}

bool Graph::connect(int fromNode, int toNode, int fromOutput, int toInput) {
    if (fromNode == toNode) return false; 
    
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    
    if (nodes.find(fromNode) == nodes.end() || nodes.find(toNode) == nodes.end()) {
        return false;
    }

    disconnect(toNode, toInput);

    Connection conn;
    conn.fromNode = fromNode;
    conn.toNode = toNode;
    conn.fromOutput = fromOutput;
    conn.toInput = toInput;
    
    connections.push_back(conn);

    if (!validateTopology()) {
        connections.pop_back();
        ofLogError("Graph") << "Cycle detected. Connection rejected.";
        return false;
    }

    Node* to = getNode(toNode);
    Node* from = getNode(fromNode);
    if (to) {
        to->setInputNode(toInput, from);
        to->onInputConnected(toInput);
        
        // Wait-free Shadow Connection (Audio)
        crumble::AudioCommand cmd;
        cmd.type = crumble::AudioCommand::CONNECT_NODES;
        cmd.targetAudioProcessor = to->getAudioProcessor();
        cmd.audioProcessor = from ? from->getAudioProcessor() : nullptr;
        cmd.toInput = toInput;
        cmd.fromOutput = fromOutput;
        pushCommand(cmd);
        
        // Wait-free Shadow Connection (Video)
        crumble::AudioCommand videoCmd;
        videoCmd.type = crumble::AudioCommand::CONNECT_NODES;
        videoCmd.targetVideoProcessor = to->getVideoProcessor();
        videoCmd.videoProcessor = from ? from->getVideoProcessor() : nullptr;
        videoCmd.toInput = toInput;
        videoCmd.fromOutput = fromOutput;
        pushCommand(videoCmd);
    }
    
    executionDirty = true;
    updateConnectionCache();
    return true;
}

void Graph::disconnect(int toNode, int toInput) {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    Node* to = getNode(toNode);
    if (to) {
        to->setInputNode(toInput, nullptr);
        
        // Wait-free Shadow Disconnection (Audio)
        crumble::AudioCommand cmd;
        cmd.type = crumble::AudioCommand::DISCONNECT_NODES;
        cmd.targetAudioProcessor = to->getAudioProcessor();
        cmd.toInput = toInput;
        pushCommand(cmd);
        
        // Wait-free Shadow Disconnection (Video)
        crumble::AudioCommand videoCmd;
        videoCmd.type = crumble::AudioCommand::DISCONNECT_NODES;
        videoCmd.targetVideoProcessor = to->getVideoProcessor();
        videoCmd.toInput = toInput;
        pushCommand(videoCmd);
    }

    connections.erase(
        std::remove_if(connections.begin(), connections.end(),
            [toNode, toInput](const Connection& c) {
                return c.toNode == toNode && c.toInput == toInput;
            }),
        connections.end()
    );
    executionDirty = true;
    updateConnectionCache();
}

void Graph::compactInputIndices(int toNode, int removedInput) {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    for (auto& conn : connections) {
        if (conn.toNode == toNode && conn.toInput > removedInput) {
            conn.toInput--;
        }
    }
    executionDirty = true;
    updateConnectionCache();
}

void Graph::removeNode(int nodeId) {
    if (nodes.find(nodeId) == nodes.end()) return;
    
    std::unique_ptr<Node> nodeToDestroy;
    bool wasDrawable = false;

    {
        std::lock_guard<std::recursive_mutex> lock(audioMutex);
        
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
        updateConnectionCache();
    }
    
    nodeToDestroy.reset(); 
    if (wasDrawable) updateRenderList();
}

void Graph::clear() {
    std::unordered_map<int, std::unique_ptr<Node>> nodesToDestroy;
    {
        std::lock_guard<std::recursive_mutex> lock(audioMutex);
        nodesToDestroy = std::move(nodes);
        connections.clear();
        cachedInputs.clear();
        cachedOutputs.clear();
        renderList.clear();
        executionDirty = true;
    }
}

std::vector<Connection> Graph::getInputConnections(int nodeId) const {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    std::vector<Connection> result;
    for (const auto& conn : connections) {
        if (conn.toNode == nodeId) result.push_back(conn);
    }
    return result;
}

std::vector<Connection> Graph::getOutputConnections(int nodeId) const {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    std::vector<Connection> result;
    for (const auto& conn : connections) {
        if (conn.fromNode == nodeId) result.push_back(conn);
    }
    return result;
}

const std::vector<Connection>& Graph::getInputConnectionsRef(int nodeId) const {
    static const std::vector<Connection> empty;
    auto it = cachedInputs.find(nodeId);
    if (it != cachedInputs.end()) return it->second;
    return empty;
}

void Graph::updateConnectionCache() {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    cachedInputs.clear();
    cachedOutputs.clear();
    for (const auto& conn : connections) {
        cachedInputs[conn.toNode].push_back(conn);
        cachedOutputs[conn.fromNode].push_back(conn);
    }
}

void Graph::update(float dt) {
    if (executionDirty) validateTopology();
    
    for (int nodeId : traversalOrder) {
        auto it = nodes.find(nodeId);
        if (it != nodes.end() && it->second) {
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
        if (node && node->canDraw) renderList.push_back(node.get());
    }
    std::sort(renderList.begin(), renderList.end(), [](Node* a, Node* b) {
        if (a->drawLayer != b->drawLayer) return a->drawLayer < b->drawLayer;
        return a->nodeId < b->nodeId;
    });
}

ofTexture* Graph::processVideo(int index) {
    for (const auto& pair : nodes) {
        Node* node = pair.second.get();
        if (node && node->type == "Outlet") {
            Outlet* outlet = static_cast<Outlet*>(node);
            if (outlet->outletIndex == index) return node->getVideoOutput();
        }
    }
    return nullptr;
}

void Graph::processAudio(ofSoundBuffer& buffer, int index) {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
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
        if (toIt != nodeIdToIndex.end()) inDegree[toIt->second]++;
    }

    std::vector<int> queue;
    for (int i = 0; i < (int)nodeIdList.size(); i++) {
        if (inDegree[i] == 0) queue.push_back(nodeIdList[i]);
    }

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
                    if (inDegree[toIdx] == 0) queue.push_back(conn.toNode);
                }
            }
        }
    }

    executionDirty = false;
    
    // UPDATE AUDIO THREAD TOPOLOGY
    if (visitedCount == nodes.size()) {
        crumble::AudioCommand cmd;
        cmd.type = crumble::AudioCommand::UPDATE_TOPOLOGY;
        // Logic for passing full traversal list would go here
        // For now, we'll keep it simple
    }

    return (visitedCount == nodes.size());
}

void Graph::registerNodeType(const std::string& type, NodeCreator creator) {
    Graph::nodeTypes[type] = creator;
}

Node* Graph::createNode(const std::string& type, const std::string& name) {
    auto it = Graph::nodeTypes.find(type);
    if (it == Graph::nodeTypes.end()) return nullptr;
    auto node = it->second();
    node->type = type;
    node->name = name.empty() ? type + "_" + std::to_string(nodes.size()) : name;
    node->nodeId = Node::nextNodeId.fetch_add(1);
    node->graph = this;
    node->drawLayer->addListener(this, &Graph::onNodeLayerChanged);
    Node* ptr = node.get();
    
    ptr->setupProcessor();

    {
        std::lock_guard<std::recursive_mutex> lock(audioMutex);
        nodes[node->nodeId] = std::move(node);
    }
    executionDirty = true;
    if (ptr->canDraw) updateRenderList();
    return ptr;
}

std::vector<std::string> Graph::getRegisteredTypes() const {
    std::vector<std::string> types;
    for (const auto& pair : Graph::nodeTypes) types.push_back(pair.first);
    return types;
}

ofJson Graph::serialize() const { return toJson(); }
void Graph::deserialize(const ofJson& json) { fromJson(json); }

ofJson Graph::toJson() const {
    ofJson json;
    ofJson nodesJson = ofJson::array();
    for (const auto& [id, node] : nodes) {
        ofJson nodeJson;
        nodeJson["id"] = node->nodeId;
        nodeJson["type"] = node->type;
        nodeJson["name"] = node->name;
        nodeJson["params"] = node->serialize();
        nodesJson.push_back(nodeJson);
    }
    ofJson connectionsJson = ofJson::array();
    for (const auto& conn : connections) {
        ofJson connJson;
        connJson["from"] = conn.fromNode;
        connJson["to"] = conn.toNode;
        connJson["fromOutput"] = conn.fromOutput;
        connJson["toInput"] = conn.toInput;
        connectionsJson.push_back(connJson);
    }
    json["version"] = "1.0";
    json["type"] = "Graph";
    json["nodes"] = nodesJson;
    json["connections"] = connectionsJson;
    ofSerialize(json["internal_params"], *parameters);
    return json;
}

bool Graph::fromJson(const ofJson& json) {
    clear();
    if (json.contains("internal_params")) ofDeserialize(json["internal_params"], *parameters);
    if (json.contains("nodes")) {
        for (const auto& nodeJson : json["nodes"]) {
            std::string type = nodeJson.value("type", "Node");
            std::string name = nodeJson.value("name", "");
            int nodeId = getSafeJson<int>(nodeJson, "id", -1);
            auto it = Graph::nodeTypes.find(type);
            if (it == Graph::nodeTypes.end()) continue;
            auto node = it->second();
            node->type = type;
            node->name = name.empty() ? type + "_" + std::to_string(nodes.size()) : name;
            node->graph = this;
            if (nodeId >= 0) {
                node->nodeId = nodeId;
                if (nodeId >= Node::nextNodeId.load()) Node::nextNodeId.store(nodeId + 1);
            } else node->nodeId = Node::nextNodeId.fetch_add(1);
            ofJson params = nodeJson.contains("params") ? nodeJson["params"] : ofJson::object();
            node->deserialize(params);
            
            node->setupProcessor();
            nodes[node->nodeId] = std::move(node);
        }
    }
    if (json.contains("connections")) {
        for (const auto& connJson : json["connections"]) {
            int from = getSafeJson<int>(connJson, "from", -1);
            int to = getSafeJson<int>(connJson, "to", -1);
            int fromOutput = getSafeJson<int>(connJson, "fromOutput", 0);
            int toInput = getSafeJson<int>(connJson, "toInput", 0);
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
    if (path.empty()) return false;
    try {
        std::string dir = ofFilePath::getEnclosingDirectory(ofToDataPath(path));
        if (!dir.empty() && !ofDirectory::doesDirectoryExist(dir)) ofDirectory::createDirectory(dir, false, true);
        ofJson json = toJson();
        std::string jsonStr = json.dump(2);
        return ofBufferToFile(path, ofBuffer(jsonStr.data(), jsonStr.size()));
    } catch (...) { return false; }
}

bool Graph::loadFromFile(const std::string& path) {
    ofJson json = ofLoadJson(path);
    if (json.empty()) return false;
    return fromJson(json);
}

std::string Graph::resolvePath(const std::string& path, const std::string& hint) const {
    if (path.empty()) return "";
    std::string resolved = AssetRegistry::get().resolve(path, hint);
    if (!resolved.empty()) return resolved;
    return ConfigManager::get().resolvePath(path);
}

void Graph::onScriptChanged(std::string& path) {
    if (!path.empty() && s_scriptExecutor) s_scriptExecutor(path, this);
}

void Graph::onNodeLayerChanged(int& layer) { updateRenderList(); }

Graph* Graph::getParentGraph() const { return graph; }
Node* Graph::getContainingNode() const { return graph; }
