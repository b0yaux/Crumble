#pragma once
#include "../../core/Node.h"
#include "ofxHapPlayer.h"

// Video file source using HAP codec for high-performance playback
// Supports both video-only and video+audio paired playback
class VideoFileSource : public Node {
public:
    VideoFileSource();
    
    void load(const std::string& videoPath, const std::string& audioPath = "");
    void loadPaired(const std::string& basePath);  // Loads basePath.mov + basePath.wav
    
    void update(float dt) override;
    ofTexture* getVideoOutput() override;
    ofSoundBuffer* getAudioOutput() override;
    
    // Playback control
    void play();
    void stop();
    void pause();
    bool isPlaying() const;
    
    // Navigation
    void setFrame(int frame);
    void setPosition(float pct);  // 0.0 to 1.0
    int getCurrentFrame() const;
    int getTotalFrames() const;
    
    // Looping
    void setLoop(bool loop);
    
private:
    ofxHapPlayer player;
    ofSoundBuffer audioBuffer;
    std::string audioFilePath;
    bool hasAudioFile = false;
    bool useEmbeddedAudio = true;
    
    // Parameters
    ofParameter<std::string> videoPath;
    ofParameter<std::string> audioPath;
    ofParameter<bool> loop;
    ofParameter<float> speed;
    ofParameter<bool> playOnLoad;
};
