#pragma once

#include <string>
#include <memory>
#include "Patterns.h"

namespace crumble {

// Compile-time FNV-1a hash for string-to-int mapping
constexpr uint32_t hashString(const char* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= static_cast<uint8_t>(*str++);
        hash *= 16777619u;
    }
    return hash;
}

class NodeProcessor;
class AudioProcessor;
class VideoProcessor;

/**
 * ProcessorCommand: A lightweight structure for wait-free communication.
 * Contains instructions from the UI/Lua thread to the shadow processor layer
 * (both AudioProcessor and VideoProcessor).  Travels through SPSC queues.
 * Note: contains shared_ptrs (displacedPattern, dataOwner) which are
 * move-only on the producer side; the hot path avoids them.
 */
struct ProcessorCommand {
    enum Type {
        NONE = 0,
        ADD_NODE,
        REMOVE_NODE,
        CONNECT_NODES,
        DISCONNECT_NODES,
        SET_PARAM,
        SET_PATTERN,        // Install a Pattern object on the audio/video thread for parameter name
        LOAD_BUFFER,
        RELEASE_BUFFER,
        SET_RELATIVE_POS,   // Jump to a relative position (0.0-1.0)
        REGISTER_ENDPOINT   // Nominate this processor as a Session-driven audio endpoint
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

    // For SET_PARAM and SET_PATTERN: 32-bit FNV-1a hash of the parameter name
    uint32_t paramHash = 0;
    
    // For SET_PATTERN: the pattern object evaluated each block.
    std::shared_ptr<Pattern<float>> pattern;
    
    // For SET_PATTERN: the old pattern being returned to the main thread for safe destruction.
    std::shared_ptr<Pattern<float>> displacedPattern;
    
    const float* audioData = nullptr;
    size_t totalSamples = 0;
    int channels = 0;
    std::shared_ptr<void> dataOwner;
    
    int fromOutput = 0;
    int toInput = 0;
};

} // namespace crumble
