#pragma once
#include "../../core/Node.h"

// Inlet: Input boundary node for nested subgraphs
// Connects to parent graph, exposes as input within child graph
class Inlet : public Node {
public:
    Inlet();
    
    ofTexture* getVideoOutput() override;
    
    // The index of this inlet (0, 1, 2, ...)
    int inletIndex = 0;
    
    // Friendly name for UI
    std::string getDisplayName() const override;
};
