#pragma once
#include "../core/Node.h"
#include "../core/Patterns.h"
#include "VideoSource.h"
#include "AudioSource.h"

// Forward declaration
class VideoEmbedAudioProcessor;

/**
 * AVSampler - Unified audio/visual sampler with synchronized playback.
 * 
 * Owns internal AudioSource and VideoSource instances with a shared
 * master playhead. All parameters (speed, loop, gain) are synchronized
 * across both sources.
 * 
 * Supports mini-notation pattern triggering via indexPattern.
 * Pattern evaluation uses query() for Tidal-style event extraction.
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
    
    // Index pattern support for mini-notation triggering
    void setIndexPattern(std::shared_ptr<Pattern<float>> pat);
    void setBankName(const std::string& name) { bankName = name; }
    std::string getBankName() const { return bankName; }
    
    // Override to handle "n" parameter for index patterns
    void modulate(const std::string& paramName, std::shared_ptr<Pattern<float>> pat) override;
    
private:
    // Internal sources - owned and synchronized by this node
    AudioSource audioSource;
    VideoSource videoSource;
    
    // Embedded audio processor (for video files with audio)
    VideoEmbedAudioProcessor* embeddedAudioProcessor = nullptr;
    
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
    double lastSyncedAudioPos = -1.0;
    
    // Embedded audio flag - true when video file contains audio
    bool useEmbeddedAudio = false;
    
    // Performance state for Sync
    bool isInternalChange = false;
    
    // Index pattern for mini-notation triggering
    std::shared_ptr<Pattern<float>> indexPattern;
    double lastQueryCycle = -1.0;
    std::string bankName;
    
    // Trigger a sample at the given index
    void triggerSample(int index);
    void silenceSample();
};
