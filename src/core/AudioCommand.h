#ifndef CRUMBLE_AUDIO_COMMAND_H
#define CRUMBLE_AUDIO_COMMAND_H

#include <string>
#include <memory>
#include "Patterns.h"

namespace crumble {

class NodeProcessor;
class AudioProcessor;
class VideoProcessor;

/**
 * AudioCommand: A POD-like structure for wait-free communication.
 * Contains instructions from the UI/Lua thread for the background threads.
 */
struct AudioCommand {
    enum Type {
        NONE = 0,
        ADD_NODE,
        REMOVE_NODE,
        CONNECT_NODES,
        DISCONNECT_NODES,
        SET_PARAM,
        SET_PATTERN,      // Install a Pattern object on the audio/video thread for parameter name
        SET_GRAPH_REF,
        LOAD_BUFFER,
        RELEASE_BUFFER
    };

    Type type = NONE;
    int nodeId = -1;
    int targetId = -1;
    float value = 0.0f;
    
    // Type-specific processor pointers
    AudioProcessor* audioProcessor = nullptr;
    AudioProcessor* targetAudioProcessor = nullptr;
    
    VideoProcessor* videoProcessor = nullptr;
    VideoProcessor* targetVideoProcessor = nullptr;

    // For SET_PARAM and SET_PATTERN: parameter name
    std::string slotName;
    
    // For SET_PATTERN: the pattern object evaluated each block.
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
