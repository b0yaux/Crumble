#include "Outlet.h"

Outlet::Outlet() {
    type = "Outlet";
    name = "Outlet";
}

ofTexture* Outlet::getVideoOutput() {
    if (!graph) return nullptr;
    
    Graph* childGraph = dynamic_cast<Graph*>(graph);
    if (!childGraph) return nullptr;
    
    auto inputs = childGraph->getInputConnections(nodeId);
    for (const auto& conn : inputs) {
        if (conn.toInput == 0) {
            Node* sourceNode = childGraph->getNode(conn.fromNode);
            if (sourceNode) {
                return sourceNode->getVideoOutput();
            }
        }
    }
    
    return nullptr;
}

void Outlet::audioOut(ofSoundBuffer& buffer) {
    if (!graph) {
        buffer.set(0);
        return;
    }
    
    Graph* childGraph = dynamic_cast<Graph*>(graph);
    if (!childGraph) {
        buffer.set(0);
        return;
    }
    
    auto inputs = childGraph->getInputConnections(nodeId);
    for (const auto& conn : inputs) {
        if (conn.toInput == 0) {
            Node* sourceNode = childGraph->getNode(conn.fromNode);
            if (sourceNode) {
                sourceNode->audioOut(buffer);
                return;
            }
        }
    }
    
    buffer.set(0);
}

std::string Outlet::getDisplayName() const {
    return "Out " + std::to_string(outletIndex);
}
