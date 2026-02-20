#include "Patch.h"

void Patch::update(float dt) {
    graph.update(dt);
}

Node* Patch::addNode(const std::string& type, const std::string& name) {
    return graph.createNode(type, name);
}

void Patch::removeNode(int index) {
    graph.removeNode(index);
}

void Patch::connect(int from, int to, int fromOut, int toIn) {
    graph.connect(from, to, fromOut, toIn);
}

void Patch::disconnect(int toNode, int toIn) {
    graph.disconnect(toNode, toIn);
}

void Patch::removeInput(int toNode, int toIn) {
    graph.removeInput(toNode, toIn);
}

Node* Patch::getNode(int index) {
    return graph.getNode(index);
}

Node* Patch::getNode(const std::string& name) {
    for (const auto& node : graph.getNodes()) {
        if (node->name == name) {
            return node.get();
        }
    }
    return nullptr;
}

int Patch::getNodeCount() const {
    return graph.getNodeCount();
}

void Patch::registerNodeType(const std::string& type, Graph::NodeCreator creator) {
    graph.registerNodeType(type, creator);
}

bool Patch::save(const std::string& path) {
    return graph.saveToFile(path);
}

bool Patch::load(const std::string& path) {
    return graph.loadFromFile(path);
}

Graph& Patch::getGraph() { return graph; }
const Graph& Patch::getGraph() const { return graph; }
