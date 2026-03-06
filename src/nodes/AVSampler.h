#pragma once
#include "../core/Node.h"
#include "VideoFileSource.h"
#include "AudioFileSource.h"

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
    ~AVSampler();
    
    void prepare(const Context& ctx) override;
    void update(float dt) override;
    void processAudio(ofSoundBuffer& buffer, int index = 0) override;
    ofTexture* processVideo(int index = 0) override;
    std::string getDisplayName() const override;
    
    crumble::NodeProcessor* createProcessor() override;
    
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
    
    // Parameters
    ofParameter<std::string> path;
    ofParameter<std::string> audioPath;
    ofParameter<std::string> videoPath;
    ofParameter<float> speed;
    ofParameter<bool> loop;
    ofParameter<bool> playing;
    ofParameter<float> position;

    // Master playhead (in samples, converted as needed for each source)
    double masterPlayhead = 0.0;
    std::string loadedAudioPath, loadedVideoPath;
    double lastSyncPos = -1.0;
    
    // Performance state for Sync
    bool isInternalChange = false;
    
    // Cached child parameters to avoid string lookups in hot paths
    ofParameter<float>* cachedAudioSpeed = nullptr;
    ofParameter<float>* cachedVideoSpeed = nullptr;
    ofParameter<float>* cachedAudioVolume = nullptr;
    ofParameter<bool>*  cachedAudioLoop = nullptr;
    ofParameter<bool>*  cachedVideoLoop = nullptr;
    ofParameter<bool>*  cachedAudioPlaying = nullptr;
    ofParameter<bool>*  cachedVideoPlaying = nullptr;
};

