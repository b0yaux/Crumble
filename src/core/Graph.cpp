#include <deque>
#include "Graph.h"
#include "Session.h"
#include "Transport.h"
#include "AssetRegistry.h"
#include "Config.h"
#include "ProcessorCommand.h"
#include "NodeProcessor.h"

// Static node type registry - shared by all Graph instances
std::map<std::string, Graph::NodeCreator> Graph::nodeTypes;

// Static script executor callback
Graph::ScriptExecutor Graph::s_scriptExecutor = nullptr;

Transport& Graph::getTransport() {
    if (rootTransport) return *rootTransport;
    // Fallback if transport not injected yet
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
    if (!active->get()) return;

    Node::prepare(ctx);
    
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    for (auto& [id, node] : nodes) {
        if (node) node->prepare(ctx);
    }
}

Node* Graph::resolveInput(int toInput) {
    for (auto& port : inlets) {
        if (port.index == toInput && port.node) return port.node;
    }
    return this;
}

Node* Graph::resolveOutput(int fromOutput) {
    for (auto& port : outlets) {
        if (port.index == fromOutput && port.node) return port.node;
    }
    return this;
}

Node* Graph::resolveAudioOutput(int fromOutput) {
    for (auto& port : outlets) {
        if (port.index == fromOutput && port.node && port.node->getAudioProcessor()) {
            return port.node;
        }
    }
    return nullptr;
}

Node* Graph::resolveVideoOutput(int fromOutput) {
    for (auto& port : outlets) {
        if (port.index == fromOutput && port.node && port.node->getVideoProcessor()) {
            return port.node;
        }
    }
    return nullptr;
}

Node* Graph::resolveAudioInput(int toInput) {
    for (auto& port : inlets) {
        if (port.index == toInput && port.node && port.node->getAudioProcessor()) {
            return port.node;
        }
    }
    return nullptr;
}

Node* Graph::resolveVideoInput(int toInput) {
    for (auto& port : inlets) {
        if (port.index == toInput && port.node && port.node->getVideoProcessor()) {
            return port.node;
        }
    }
    return nullptr;
}

bool Graph::connect(int fromNode, int toNode, int fromOutput, int toInput) {
    if (fromNode == toNode) return false; 
    
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    
    if (nodes.find(fromNode) == nodes.end() || nodes.find(toNode) == nodes.end()) {
        return false;
    }

    // Check if this exact connection already exists to prevent stacking
    for (auto& conn : connections) {
        if (conn.fromNode == fromNode && conn.toNode == toNode && 
            conn.fromOutput == fromOutput && conn.toInput == toInput) {
            conn.stale = false; // Mark as active
            return true; 
        }
    }

    disconnect(toNode, toInput);

    Connection conn;
    conn.fromNode = fromNode;
    conn.toNode = toNode;
    conn.fromOutput = fromOutput;
    conn.toInput = toInput;
    conn.stale = false; 
    
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
        
        Node* toAudio = to->resolveAudioInput(toInput);
        Node* toVideo = to->resolveVideoInput(toInput);
        Node* fromAudio = from ? from->resolveAudioOutput(fromOutput) : nullptr;
        Node* fromVideo = from ? from->resolveVideoOutput(fromOutput) : nullptr;
        
        // Wait-free Shadow Connection (Audio)
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::CONNECT_NODES;
        cmd.targetAudioProcessor = toAudio ? toAudio->getAudioProcessor() : nullptr;
        cmd.audioProcessor = fromAudio ? fromAudio->getAudioProcessor() : nullptr;
        cmd.nodeId = fromAudio ? fromAudio->nodeId : -1;
        cmd.targetId = toAudio ? toAudio->nodeId : -1;
        cmd.toInput = toInput;
        cmd.fromOutput = fromOutput;
        pushCommand(cmd);
        
        // Wait-free Shadow Connection (Video)
        crumble::ProcessorCommand videoCmd;
        videoCmd.type = crumble::ProcessorCommand::CONNECT_NODES;
        videoCmd.targetVideoProcessor = toVideo ? toVideo->getVideoProcessor() : nullptr;
        videoCmd.videoProcessor = fromVideo ? fromVideo->getVideoProcessor() : nullptr;
        videoCmd.nodeId = fromVideo ? fromVideo->nodeId : -1;
        videoCmd.targetId = toVideo ? toVideo->nodeId : -1;
        videoCmd.toInput = toInput;
        videoCmd.fromOutput = fromOutput;
        pushCommand(videoCmd);
    }
    
    executionDirty = true;
    return true;
}

void Graph::markConnectionsStale() {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    for (auto& conn : connections) {
        conn.stale = true;
    }
}

void Graph::pruneStaleConnections() {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    std::vector<std::pair<int, int>> toRemove;
    for (const auto& conn : connections) {
        if (conn.stale) {
            toRemove.push_back({conn.toNode, conn.toInput});
        }
    }
    
    ofLogNotice("Graph") << "pruneStaleConnections: removing " << toRemove.size() << " stale connections";
    for (auto& pair : toRemove) {
        disconnect(pair.first, pair.second);
    }
}

void Graph::disconnect(int toNode, int toInput) {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    Node* to = getNode(toNode);
    if (to) {
        to->setInputNode(toInput, nullptr);
        
        Node* toAudio = to->resolveAudioInput(toInput);
        Node* toVideo = to->resolveVideoInput(toInput);
        
        // Wait-free Shadow Disconnection (Audio)
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::DISCONNECT_NODES;
        cmd.targetAudioProcessor = toAudio ? toAudio->getAudioProcessor() : nullptr;
        cmd.targetId = toAudio ? toAudio->nodeId : -1;
        cmd.toInput = toInput;
        pushCommand(cmd);
        
        // Wait-free Shadow Disconnection (Video)
        crumble::ProcessorCommand videoCmd;
        videoCmd.type = crumble::ProcessorCommand::DISCONNECT_NODES;
        videoCmd.targetVideoProcessor = toVideo ? toVideo->getVideoProcessor() : nullptr;
        videoCmd.targetId = toVideo ? toVideo->nodeId : -1;
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
}

void Graph::compactInputIndices(int toNode, int removedInput) {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    for (auto& conn : connections) {
        if (conn.toNode == toNode && conn.toInput > removedInput) {
            conn.toInput--;
        }
    }
    executionDirty = true;
}

void Graph::removeNode(int nodeId) {
    auto it = nodes.find(nodeId);
    if (it == nodes.end()) return;
    
    std::unique_ptr<Node> nodeToDestroy;
    bool wasDrawable = false;

    {
        std::lock_guard<std::recursive_mutex> lock(audioMutex);
        
        std::vector<std::pair<int, int>> toDisconnect;
        for (const auto& conn : connections) {
            if (conn.fromNode == nodeId || conn.toNode == nodeId) {
                toDisconnect.push_back({conn.toNode, conn.toInput});
            }
        }

        for (const auto& pair : toDisconnect) {
            disconnect(pair.first, pair.second);
        }

        auto pruneBoundaries = [nodeId](std::vector<Graph::Boundary>& ports) {
            ports.erase(
                std::remove_if(ports.begin(), ports.end(),
                    [nodeId](const Graph::Boundary& p) { return p.node && p.node->nodeId == nodeId; }),
                ports.end());
        };
        pruneBoundaries(outlets);
        pruneBoundaries(inlets);

        for (auto& [paramName, targets] : proxyParams) {
            targets.erase(
                std::remove_if(targets.begin(), targets.end(),
                    [nodeId](const ProxyTarget& t) { return t.childId == nodeId; }),
                targets.end());
        }

        nodeToDestroy = std::move(it->second);
        wasDrawable = nodeToDestroy->canDraw;
        nodes.erase(it);
        
        executionDirty = true;
    }
    
    nodeToDestroy.reset(); 
    if (wasDrawable) updateRenderList();
}

void Graph::clear() {
    std::unordered_map<int, std::unique_ptr<Node>> nodesToDestroy;
    {
        std::lock_guard<std::recursive_mutex> lock(audioMutex);
        
        for (const auto& conn : connections) {
            Node* to = getNode(conn.toNode);
            if (to) {
                Node* toAudio = to->resolveAudioInput(conn.toInput);
                Node* toVideo = to->resolveVideoInput(conn.toInput);
                
                crumble::ProcessorCommand cmd;
                cmd.type = crumble::ProcessorCommand::DISCONNECT_NODES;
                cmd.targetAudioProcessor = toAudio ? toAudio->getAudioProcessor() : nullptr;
                cmd.targetId = toAudio ? toAudio->nodeId : -1;
                cmd.toInput = conn.toInput;
                pushCommand(cmd);
                
                crumble::ProcessorCommand videoCmd;
                videoCmd.type = crumble::ProcessorCommand::DISCONNECT_NODES;
                videoCmd.targetVideoProcessor = toVideo ? toVideo->getVideoProcessor() : nullptr;
                videoCmd.targetId = toVideo ? toVideo->nodeId : -1;
                videoCmd.toInput = conn.toInput;
                pushCommand(videoCmd);
            }
        }
        
        nodesToDestroy = std::move(nodes);
        connections.clear();
        renderList.clear();
        outlets.clear();
        inlets.clear();
        proxyParams.clear();
        if (onClear) onClear();
        onUpdate = nullptr;
        onClear = nullptr;
        executionDirty = true;
    }
}

void Graph::clearEphemeral() {
    if (onClear) {
        onClear();
        onClear = nullptr;
    }
    onUpdate = nullptr;
    outlets.clear();
    inlets.clear();
    proxyParams.clear();
}

void Graph::beginScript() {
    for (auto& [nodeId, node] : nodes) {
        node->touched = false;
    }
}

int Graph::endScript() {
    std::vector<int> nodeIds;
    nodeIds.reserve(nodes.size());
    for (const auto& [id, node] : nodes) {
        nodeIds.push_back(id);
    }
    int removedCount = 0;
    for (int nodeId : nodeIds) {
        if (auto node = getNode(nodeId)) {
            if (!node->touched) {
                removeNode(nodeId);
                removedCount++;
            }
        }
    }
    return removedCount;
}

void Graph::update(float dt) {
    if (!active->get()) return;

    if (executionDirty) validateTopology();
    
    if (onUpdate) onUpdate();

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
        if (a->drawLayer->get() != b->drawLayer->get()) return a->drawLayer->get() < b->drawLayer->get();
        return a->nodeId < b->nodeId;
    });
}

ofTexture* Graph::processVideo(int index) {
    Node* videoNode = resolveVideoOutput(index);
    if (videoNode) return videoNode->getVideoOutput();
    return nullptr;
}

void Graph::processAudio(ofSoundBuffer& buffer, int index) {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    Node* audioNode = resolveAudioOutput(index);
    if (audioNode) {
        audioNode->pullAudio(buffer);
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

    std::deque<int> queue;
    for (int i = 0; i < (int)nodeIdList.size(); i++) {
        if (inDegree[i] == 0) queue.push_back(nodeIdList[i]);
    }

    size_t visitedCount = 0;
    while (!queue.empty()) {
        int currentNodeId = queue.front();
        queue.pop_front();
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
    return (visitedCount == nodes.size());
}

void Graph::registerNodeType(const std::string& type, NodeCreator creator) {
    Graph::nodeTypes[type] = creator;
}

Node* Graph::createNode(const std::string& type, const std::string& name) {
    // 1. Check for existing node with same name and type for idempotency
    if (!name.empty()) {
        std::lock_guard<std::recursive_mutex> lock(audioMutex);
        for (auto& [id, node] : nodes) {
            if (node && node->name == name && node->type == type) {
                return node.get(); // Reuse existing node
            }
        }
    }

    // 2. Create new node if not found
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

    // If it's a subgraph, inject the transport from the parent graph
    if (auto* subGraph = dynamic_cast<Graph*>(ptr)) {
        subGraph->setTransport(rootTransport);
    }

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

AudioCache* Graph::getCache() const {
    if (g_session) return &g_session->getCache();
    if (graph) return graph->getCache(); // nested subgraph: walk up to the root
    return nullptr;
}

int Graph::getSampleRate() const {
    if (g_session) return g_session->getSampleRate();
    if (graph) return graph->getSampleRate();
    return 44100;
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
Node* Graph::getContainingNode() const { return const_cast<Graph*>(this); }

void Graph::addOutlet(int nodeId, int index) {
    Node* node = getNode(nodeId);
    if (!node || node->graph != this) return;

    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    outlets.push_back({index, node});
}

void Graph::addInlet(int nodeId, int index) {
    Node* node = getNode(nodeId);
    if (!node || node->graph != this) return;

    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    inlets.push_back({index, node});
}

void Graph::addProxyTarget(const std::string& parentParam, int childId, const std::string& childParam) {
    // Allow multiple children to share the same parent param name
    // (e.g. expose("path", a) + expose("path", v) both register under "path").
    proxyParams[parentParam].push_back({childId, childParam});
}

std::vector<Graph::ProxyTarget> Graph::getProxyTargets(const std::string& parentParam) const {
    auto it = proxyParams.find(parentParam);
    if (it != proxyParams.end()) return it->second;
    return {};
}

void Graph::onParameterChanged(const std::string& paramName) {
    if (paramName == "script") return;
    if (paramName == "active") {
        bool val = active->get();
        std::lock_guard<std::recursive_mutex> lock(audioMutex);
        for (auto& [id, node] : nodes) {
            if (node) {
                node->active->set(val);
                node->onParameterChanged("active");
            }
        }
        return;
    }
    Node::onParameterChanged(paramName);
}
