#pragma once
#include "../core/Node.h"
#include "ofxHapPlayer.h"

// Video file source using HAP codec for high-performance playback
class VideoFileSource : public Node {
public:
    VideoFileSource();
    
    void load(const std::string& path);

    void update(float dt) override;
    ofTexture* getVideoOutput(int index = 0) override;

    // Returns filename for UI display
    std::string getDisplayName() const override;
    
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
    
    // Serialization
    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;
    
    // React to parameter changes from Lua
    void onParameterChanged(const std::string& paramName) override;
    
private:
    void onPathChanged(std::string& path);
    ofxHapPlayer player;
    std::string loadedPath;
    
    // Parameters
    ofParameter<std::string> path;
    ofParameter<bool> loop;
    ofParameter<float> speed;
    ofParameter<bool> playOnLoad;
};
