#pragma once
#include "../../core/Node.h"
#include "../../core/Graph.h"

// Outlet: Output boundary node for nested subgraphs
// Acts as a sink inside child graph; nodes connect TO Outlet
// Exposes connected node's output to parent graph
class Outlet : public Node {
public:
    Outlet();
    
    ofTexture* getVideoOutput() override;
    void audioOut(ofSoundBuffer& buffer) override;
    
    // The index of this outlet (0, 1, 2, ...)
    int outletIndex = 0;
    
    // Friendly name for UI
    std::string getDisplayName() const override;
};
