#pragma once
#include "../../core/Node.h"

// Inlet: Input boundary node for nested subgraphs
// Acts as a source inside child graph, pulling from parent graph connections
// When parent connects: parentNode -> Graph(Inlet), Inlet provides that data internally
//
// ARCHITECTURAL NOTE: Inlet has no shadow AudioProcessor/VideoProcessor.
// processAudio() runs on the audio thread but acquires audioMutex via
// Graph::getInputConnections(), bypassing the shadow-processor isolation.
// This is safe only because subgraphs are currently not used in production
// real-time audio patches. If that changes, Inlet/Outlet need shadow
// processor implementations that route through the SPSC command queues.
class Inlet : public Node {
public:
    Inlet();
    
    crumble::AudioProcessor* createAudioProcessor() override;
    crumble::VideoProcessor* createVideoProcessor() override;
    
    // The index of this inlet (0, 1, 2, ...)
    ofParameter<int> inletIndex;
    
    // Friendly name for UI
    std::string getDisplayName() const override;
};
