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
 * SET_PATTERN carries a shared_ptr<Pattern<float>> to the audio thread.
 * The audio thread stores it in processor->patternSlots[slotIndex] and
 * evaluates it inline per sample using the live cycle position.
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
        SET_PATTERN,      // Install a Pattern object on the audio thread for slot N
        SET_SLOT,         // Register a name->index mapping in processor->slotMap
        SET_GRAPH_REF,
        UPDATE_TOPOLOGY,
        LOAD_BUFFER,
        RELEASE_BUFFER
    };

    Type type = NONE;
    int nodeId = -1;
    int targetId = -1;
    int slotIndex = -1;
    float value = 0.0f;
    
    // Topology and Pointers
    NodeProcessor* processor = nullptr;
    NodeProcessor* targetProcessor = nullptr;

    // For SET_PATTERN: the pattern object evaluated by the audio thread each block.
    // shared_ptr ref-count is atomic — safe to copy across the thread boundary.
    std::shared_ptr<Pattern<float>> pattern;
    
    // For LOAD_BUFFER
    const float* audioData = nullptr;
    size_t totalSamples = 0;
    int channels = 0;
    
    int fromOutput = 0;
    int toInput = 0;

    // For SET_SLOT: parameter name (small-string-optimized, safe to copy)
    std::string slotName;
};

} // namespace crumble

#endif // CRUMBLE_AUDIO_COMMAND_H
