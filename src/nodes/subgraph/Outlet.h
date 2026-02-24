#pragma once
#include "../../core/Node.h"

// Outlet: Output boundary node for nested subgraphs
// Exposes as output within child graph, connects to parent graph
class Outlet : public Node {
public:
    Outlet();
    
    ofTexture* getVideoOutput() override;
    
    // The index of this outlet (0, 1, 2, ...)
    int outletIndex = 0;
    
    // Friendly name for UI
    std::string getDisplayName() const override;
};
