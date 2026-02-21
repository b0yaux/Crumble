#include "Session.h"

Session* g_session = nullptr;

// --- Graph primitives ---

Node* Session::addNode(const std::string& type, const std::string& name) {
    return graph.createNode(type, name);
}

void Session::removeNode(int nodeId) {
    graph.removeNode(nodeId);
}

void Session::connect(int fromNode, int toNode, int fromOutput, int toInput) {
    graph.connect(fromNode, toNode, fromOutput, toInput);
}

void Session::disconnect(int toNode, int toInput) {
    graph.disconnect(toNode, toInput);
}

void Session::removeInput(int toNode, int toInput) {
    graph.removeInput(toNode, toInput);
}

void Session::clear() {
    graph.clear();
}

// --- Script lifecycle ---

void Session::beginScript() {
    for (auto& [nodeId, node] : graph.getNodes()) {
        node->touched = false;
    }
}

void Session::endScript() {
    // Remove untouched nodes - iterate over a copy of node IDs since we modify the map
    std::vector<int> nodeIds;
    for (const auto& [id, node] : graph.getNodes()) {
        nodeIds.push_back(id);
    }
    
    for (int nodeId : nodeIds) {
        if (auto node = graph.getNode(nodeId)) {
            if (!node->touched) {
                graph.removeNode(nodeId);
            }
        }
    }
}

void Session::touchNode(int nodeId) {
    if (auto node = graph.getNode(nodeId)) {
        node->touched = true;
    }
}

// --- Lifecycle ---

void Session::update(float dt) {
    graph.update(dt);
}

// --- Node access ---

Node* Session::getNode(int nodeId) {
    return graph.getNode(nodeId);
}

Node* Session::findNodeByName(const std::string& name) {
    for (const auto& [id, node] : graph.getNodes()) {
        if (node->name == name) return node.get();
    }
    return nullptr;
}

int Session::getNodeCount() const {
    return static_cast<int>(graph.getNodeCount());
}

// --- Persistence ---

bool Session::save(const std::string& path) {
    return graph.saveToFile(path);
}

bool Session::load(const std::string& path) {
    return graph.loadFromFile(path);
}

// --- Factory ---

void Session::registerNodeType(const std::string& type, Graph::NodeCreator creator) {
    graph.registerNodeType(type, creator);
}

std::vector<std::string> Session::getRegisteredTypes() const {
    return graph.getRegisteredTypes();
}

// --- Direct graph access ---

Graph& Session::getGraph() { return graph; }
const Graph& Session::getGraph() const { return graph; }
