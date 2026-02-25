#include "Inlet.h"

Inlet::Inlet() {
    type = "Inlet";
    name = "Inlet";
}

ofTexture* Inlet::getVideoOutput() {
    if (!graph) return nullptr;
    
    Graph* childGraph = dynamic_cast<Graph*>(graph);
    if (!childGraph) return nullptr;
    
    Node* containingNode = childGraph->getContainingNode();
    if (!containingNode) return nullptr;
    
    Graph* parentGraph = childGraph->getParentGraph();
    if (!parentGraph) return nullptr;
    
    auto inputs = parentGraph->getInputConnections(containingNode->nodeId);
    for (const auto& conn : inputs) {
        if (conn.toInput == inletIndex) {
            Node* source = parentGraph->getNode(conn.fromNode);
            if (source) return source->getVideoOutput();
        }
    }
    
    return nullptr;
}

void Inlet::audioOut(ofSoundBuffer& buffer) {
    if (!graph) {
        buffer.set(0);
        return;
    }
    
    Graph* childGraph = dynamic_cast<Graph*>(graph);
    if (!childGraph) {
        buffer.set(0);
        return;
    }
    
    Node* containingNode = childGraph->getContainingNode();
    if (!containingNode) {
        buffer.set(0);
        return;
    }
    
    Graph* parentGraph = childGraph->getParentGraph();
    if (!parentGraph) {
        buffer.set(0);
        return;
    }
    
    auto inputs = parentGraph->getInputConnections(containingNode->nodeId);
    for (const auto& conn : inputs) {
        if (conn.toInput == inletIndex) {
            Node* source = parentGraph->getNode(conn.fromNode);
            if (source) {
                source->audioOut(buffer);
                return;
            }
        }
    }
    
    buffer.set(0);
}

std::string Inlet::getDisplayName() const {
    return "In " + std::to_string(inletIndex);
}
