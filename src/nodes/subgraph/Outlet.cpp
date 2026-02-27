#include "Outlet.h"

Outlet::Outlet() {
    type = "Outlet";
    name = "Outlet";
    parameters.add(outletIndex.set("outletIndex", 0, 0, 16));
}

ofTexture* Outlet::getVideoOutput(int idx) {
    if (!graph) return nullptr;
    
    Graph* childGraph = dynamic_cast<Graph*>(graph);
    if (!childGraph) return nullptr;
    
    auto inputs = childGraph->getInputConnections(nodeId);
    for (const auto& conn : inputs) {
        if (conn.toInput == 0) {
            Node* source = childGraph->getNode(conn.fromNode);
            if (source) return source->getVideoOutput(conn.fromOutput);
        }
    }
    
    return nullptr;
}

void Outlet::pullAudio(ofSoundBuffer& buffer, int idx) {
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
            Node* source = childGraph->getNode(conn.fromNode);
            if (source) {
                source->pullAudio(buffer, conn.fromOutput);
                return;
            }
        }
    }
    
    buffer.set(0);
}

std::string Outlet::getDisplayName() const {
    return "Out " + std::to_string(outletIndex);
}
