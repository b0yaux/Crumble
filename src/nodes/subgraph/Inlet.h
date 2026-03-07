#pragma once
#include "../../core/Node.h"

// Inlet: Input boundary node for nested subgraphs
// Acts as a source inside child graph, pulling from parent graph connections
// When parent connects: parentNode -> Graph(Inlet), Inlet provides that data internally
class Inlet : public Node {
public:
    Inlet();
    
    ofTexture* processVideo(int index = 0) override;
    void processAudio(ofSoundBuffer& buffer, int index = 0) override;
    
    // The index of this inlet (0, 1, 2, ...)
    ofParameter<int> inletIndex;
    
    // Friendly name for UI
    std::string getDisplayName() const override;
};
