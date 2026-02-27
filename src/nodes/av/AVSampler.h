#pragma once
#include "../../core/Node.h"
#include "../video/VideoFileSource.h"
#include "../audio/AudioFileSource.h"

/**
 * AVSampler - Unified audio/visual sampler with synchronized playback.
 * 
 * Owns internal AudioFileSource and VideoFileSource instances with a shared
 * master playhead. All parameters (speed, loop, volume) are synchronized
 * across both sources.
 * 
 * Provides:
 * - Unified speed control (affects both audio and video equally)
 * - Unified loop control
 * - Audio volume control
 * - Master playhead management (ensures A/V sync)
 * - Flexibility to load different audio/video independently via live-scripting
 */
class AVSampler : public Node {
public:
    AVSampler();
    
    // Node interface
    void update(float dt) override;
    void pullAudio(ofSoundBuffer& buffer, int index = 0) override;
    ofTexture* getVideoOutput(int index = 0) override;
    std::string getDisplayName() const override;
    
    // Serialization
    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;
    
    // React to parameter changes from Lua
    void onParameterChanged(const std::string& paramName) override;
    
    // Direct access to internal sources (for advanced use via Lua)
    AudioFileSource* getAudioSource() { return &audioSource; }
    VideoFileSource* getVideoSource() { return &videoSource; }
    
    // Master playhead access
    double getMasterPlayhead() const { return masterPlayhead; }
    void setMasterPlayhead(double position);
    
private:
    // Internal sources - owned and synchronized by this node
    AudioFileSource audioSource;
    VideoFileSource videoSource;
    
    // Master playhead (in samples, converted as needed for each source)
    double masterPlayhead = 0.0;
    
    // Unified parameters
    ofParameter<std::string> audioPath;
    ofParameter<std::string> videoPath;
    ofParameter<float> speed;      // Shared speed parameter
    ofParameter<float> volume;     // Audio volume
    ofParameter<bool> loop;        // Shared loop state
    ofParameter<bool> playing;     // Shared playback state
    
    // Future: 
    // ofParameter<float> position;  // Start position
    // ofParameter<float> duration;  // Play duration before stop/loop
    // ofParameterGroup envelope;    // Envelope control
    
    std::string loadedAudioPath;
    std::string loadedVideoPath;
};
