#pragma once
#include "../../core/Node.h"

// Outlet: Output boundary node for nested subgraphs
// Acts as a sink inside child graph; nodes connect TO Outlet
// Exposes connected node's output to parent graph
//
// ARCHITECTURAL NOTE: Outlet has no shadow AudioProcessor/VideoProcessor.
// processAudio() runs on the audio thread but acquires audioMutex via
// Graph::getInputConnections(), bypassing the shadow-processor isolation.
// This is safe only because subgraphs are currently not used in production
// real-time audio patches. If that changes, Inlet/Outlet need shadow
// processor implementations that route through the SPSC command queues.
class Outlet : public Node {
public:
    Outlet();
    
    ofTexture* processVideo(int index = 0) override;
    void processAudio(ofSoundBuffer& buffer, int index = 0) override;
    
    // The index of this outlet (0, 1, 2, ...)
    ofParameter<int> outletIndex;
    
    // Friendly name for UI
    std::string getDisplayName() const override;
};
