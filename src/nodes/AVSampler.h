#pragma once
#include "../core/Node.h"
#include "VideoSource.h"
#include "AudioSource.h"

/**
 * AVSampler - Unified audio/visual sampler with synchronized playback.
 * 
 * Owns internal AudioSource and VideoSource instances with a shared
 * master playhead. All parameters (speed, loop, gain) are synchronized
 * across both sources.
 * 
 * Supports pattern-based triggering via the "n" parameter.
 * Uses unified pattern system: patterns flow through ControlSlot,
 * triggers are evaluated sample-accurately in audio thread.
 * 
 * AudioSourceProcessor handles both standalone audio buffers and
 * embedded audio decoded from video files via LOAD_BUFFER command.
 */
class AVSampler : public Node {
public:
    AVSampler();
    ~AVSampler();
    
    void prepare(const Context& ctx) override;
    void update(float dt) override;
    void setupProcessor() override;
    void processAudio(ofSoundBuffer& buffer, int index = 0) override;
    ofTexture* processVideo(int index = 0) override;
    std::string getDisplayName() const override;
    
    crumble::AudioProcessor* createAudioProcessor() override;
    crumble::VideoProcessor* createVideoProcessor() override;
    
    // Serialization
    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;
    
    // React to parameter changes from Lua
    void onParameterChanged(const std::string& paramName) override;

    // Direct access to internal sources (for advanced use via Lua)
    AudioSource* getAudioSource() { return &audioSource; }
    VideoSource* getVideoSource() { return &videoSource; }
    
    // Master playhead access
    double getMasterPlayhead() const { return masterPlayhead; }
    void setMasterPlayhead(double position);
    
    // Trigger a sample at the given index
    void triggerSample(int index);
    void triggerSampleWithPath(const std::string& resolvedPath);
    void silenceSample();
    
private:
    // Internal sources - owned and synchronized by this node
    AudioSource audioSource;
    VideoSource videoSource;
    
    // Parameters
    ofParameter<std::string> path;
    ofParameter<std::string> audioPath;
    ofParameter<std::string> videoPath;
    ofParameter<float> speed;
    ofParameter<bool> loop;
    ofParameter<bool> playing;
    ofParameter<float> position;
    ofParameter<float> triggerPosition;

    // Master playhead (in samples, converted as needed for each source)
    double masterPlayhead = 0.0;
    std::string loadedAudioPath, loadedVideoPath;
};
