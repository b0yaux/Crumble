#include "Inlet.h"

Inlet::Inlet() {
    type = "Inlet";
    name = "Inlet";
}

ofTexture* Inlet::getVideoOutput() {
    if (!graph) return nullptr;
    
    Graph* childGraph = dynamic_cast<Graph*>(graph);
    if (!childGraph) return nullptr;
    
    Graph* parentGraphNode = dynamic_cast<Graph*>(childGraph->graph);
    if (!parentGraphNode) return nullptr;
    
    Graph* parentGraph = parentGraphNode->graph;
    if (!parentGraph) return nullptr;
    
    auto inputs = parentGraph->getInputConnections(parentGraphNode->nodeId);
    for (const auto& conn : inputs) {
        if (conn.toInput == inletIndex) {
            Node* sourceNode = parentGraph->getNode(conn.fromNode);
            if (sourceNode) {
                return sourceNode->getVideoOutput();
            }
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
    
    Graph* parentGraphNode = dynamic_cast<Graph*>(childGraph->graph);
    if (!parentGraphNode) {
        buffer.set(0);
        return;
    }
    
    Graph* parentGraph = parentGraphNode->graph;
    if (!parentGraph) {
        buffer.set(0);
        return;
    }
    
    auto inputs = parentGraph->getInputConnections(parentGraphNode->nodeId);
    for (const auto& conn : inputs) {
        if (conn.toInput == inletIndex) {
            Node* sourceNode = parentGraph->getNode(conn.fromNode);
            if (sourceNode) {
                sourceNode->audioOut(buffer);
                return;
            }
        }
    }
    
    buffer.set(0);
}

std::string Inlet::getDisplayName() const {
    return "In " + std::to_string(inletIndex);
}
