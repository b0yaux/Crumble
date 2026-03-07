#pragma once
#include "../core/Node.h"
#include "ofxHapPlayer.h"

// Video file source using HAP codec for high-performance playback
class VideoFileSource : public Node {
public:
    VideoFileSource();
    
    // Performance: Members made public to allow AVSampler direct pointer access
    ofParameter<std::string> path;
    ofParameter<float> speed;
    ofParameter<bool> loop;
    ofParameter<bool> playing;
    ofParameter<int> clockMode;

    crumble::VideoProcessor* createVideoProcessor() override;
    ofTexture* processVideo(int index = 0) override;

    // Returns filename for UI display
    std::string getDisplayName() const override;
    
    // Playback control
    enum ClockMode {
        INTERNAL,
        EXTERNAL
    };
    
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
    
    // Serialization
    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;
    
    // React to parameter changes from Lua
    void onParameterChanged(const std::string& paramName) override;
    
    void load(const std::string& path);

private:
    void onPathChanged(std::string& path);
    void onClockModeChanged(int& mode);
    std::string loadedPath;
    ofxHapPlayer player;
};
