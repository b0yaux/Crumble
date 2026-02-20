#include "Session.h"
#include <algorithm>

Session* g_session = nullptr;

Session::Session() {
    g_session = this;
}

Node* Session::addNode(const std::string& type, const std::string& name) {
    return graph.createNode(type, name);
}

void Session::removeNode(int index) {
    graph.removeNode(index);
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
    checkpoint();
    graph.clear();
    assetPool.prune(5.0f); // Clean up assets that are no longer referenced
}

// --- Lifecycle ---

void Session::update(float dt) {
    graph.update(dt);
}

// --- Node access ---

Node* Session::getNode(int index) {
    return graph.getNode(index);
}

Node* Session::findNodeByName(const std::string& name) {
    for (const auto& node : graph.getNodes()) {
        if (node->name == name) return node.get();
    }
    return nullptr;
}

int Session::getNodeCount() const {
    return static_cast<int>(graph.getNodeCount());
}

// --- Graph output routing ---

void Session::setVideoOutputNode(int nodeIndex) {
    graph.setVideoOutputNode(nodeIndex);
}

void Session::setAudioOutputNode(int nodeIndex) {
    graph.setAudioOutputNode(nodeIndex);
}

// --- Undo / Redo ---

void Session::checkpoint() {
    // Truncate any redo future
    if (snapshotPos < (int)snapshots.size() - 1) {
        snapshots.erase(snapshots.begin() + snapshotPos + 1, snapshots.end());
    }

    snapshots.push_back(graph.toJson());
    snapshotPos++;

    // Cap history size
    if (snapshots.size() > maxSnapshots) {
        snapshots.erase(snapshots.begin());
        snapshotPos--;
    }
}

void Session::undo() {
    if (!canUndo()) return;
    // Before first undo, save current state so redo can restore it
    if (snapshotPos == (int)snapshots.size() - 1) {
        snapshots.push_back(graph.toJson());
    }
    graph.fromJson(snapshots[snapshotPos]);
    snapshotPos--;
}

void Session::redo() {
    if (!canRedo()) return;
    snapshotPos++;
    // The state to restore is one ahead of snapshotPos
    graph.fromJson(snapshots[snapshotPos + 1]);
}

bool Session::canUndo() const {
    return snapshotPos >= 0;
}

bool Session::canRedo() const {
    return snapshotPos < (int)snapshots.size() - 2;
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
