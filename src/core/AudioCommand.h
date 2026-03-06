#ifndef CRUMBLE_AUDIO_COMMAND_H
#define CRUMBLE_AUDIO_COMMAND_H

#include <string>
#include <memory>
#include "Patterns.h"

namespace crumble {

class NodeProcessor;

/**
 * AudioCommand: A POD-like structure for wait-free communication.
 * Contains instructions from the UI/Lua thread for the Audio thread.
 *
 * SET_PARAM and SET_PATTERN use slotName (parameter name) instead of index.
 * This eliminates index confusion between different node types.
 * Patterns are stateless (pure cycle->value functions) and therefore
 * fully thread-safe to call from any thread without locking.
 */
struct AudioCommand {
    enum Type {
        NONE = 0,
        ADD_NODE,
        REMOVE_NODE,
        CONNECT_NODES,
        DISCONNECT_NODES,
        SET_PARAM,
        SET_PATTERN,      // Install a Pattern object on the audio thread for parameter name
        SET_GRAPH_REF,
        UPDATE_TOPOLOGY,
        LOAD_BUFFER,
        RELEASE_BUFFER
    };

    Type type = NONE;
    int nodeId = -1;
    int targetId = -1;
    float value = 0.0f;
    
    // Topology and Pointers
    NodeProcessor* processor = nullptr;
    NodeProcessor* targetProcessor = nullptr;

    // For SET_PARAM and SET_PATTERN: parameter name (not index!)
    std::string slotName;
    
    // For SET_PATTERN: the pattern object evaluated by the audio thread each block.
    // shared_ptr ref-count is atomic — safe to copy across the thread boundary.
    std::shared_ptr<Pattern<float>> pattern;
    
    // For LOAD_BUFFER
    const float* audioData = nullptr;
    size_t totalSamples = 0;
    int channels = 0;
    
    int fromOutput = 0;
    int toInput = 0;
};

} // namespace crumble

#endif // CRUMBLE_AUDIO_COMMAND_H
