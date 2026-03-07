#ifndef CRUMBLE_PROCESSOR_COMMAND_H
#define CRUMBLE_PROCESSOR_COMMAND_H

#include <string>
#include <memory>
#include "Patterns.h"

namespace crumble {

class NodeProcessor;
class AudioProcessor;
class VideoProcessor;

/**
 * ProcessorCommand: A POD-like structure for wait-free communication.
 * Contains instructions from the UI/Lua thread to the shadow processor layer
 * (both AudioProcessor and VideoProcessor).  Travels through SPSC queues;
 * keep it cheap to copy and free of non-trivial destructors on the hot path.
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
        SET_GRAPH_REF,
        LOAD_BUFFER,
        RELEASE_BUFFER,
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

    // For SET_PARAM and SET_PATTERN: parameter name
    std::string slotName;
    
    // For SET_PATTERN: the pattern object evaluated each block.
    std::shared_ptr<Pattern<float>> pattern;
    
    // For LOAD_BUFFER / RELEASE_BUFFER
    // dataOwner keeps the underlying audio buffer alive for exactly as long as
    // the AudioFileProcessor holds a reference to it.  The processor stores this
    // shared_ptr alongside the raw audioData pointer; the raw pointer becomes
    // safe to dereference for the processor's entire lifetime without any
    // explicit synchronisation between the UI thread and the audio thread.
    const float* audioData = nullptr;
    size_t totalSamples = 0;
    int channels = 0;
    std::shared_ptr<void> dataOwner; // type-erased to avoid pulling ofxAudioFile here
    
    int fromOutput = 0;
    int toInput = 0;
};

} // namespace crumble

#endif // CRUMBLE_PROCESSOR_COMMAND_H
