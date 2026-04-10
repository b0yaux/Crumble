#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
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
 * TriggerMap: immutable ref-to-index mapping for trigger patterns.
 * Built at set time, sent to audio thread via SET_TRIGGER_MAP command.
 * Allows the audio thread to convert Event{ref="k"} into an integer
 * without string copy or mutex.
 */
struct TriggerMap {
    std::unordered_map<std::string, int> refToIndex;
};

using TriggerMapPtr = std::shared_ptr<TriggerMap>;

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
        SET_TRIGGER_MAP,   // Install ref→index mapping for trigger patterns
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
    
    // For SET_TRIGGER_MAP: ref→index mapping for trigger patterns
    TriggerMapPtr triggerMap;
    
    int fromOutput = 0;
    int toInput = 0;
};

} // namespace crumble
