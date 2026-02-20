#pragma once
#include "../../core/Node.h"
#include "ofxHapPlayer.h"

// Video file source using HAP codec for high-performance playback
class VideoFileSource : public Node {
public:
    VideoFileSource();
    
    void load(const std::string& videoPath);
    
    void update(float dt) override;
    ofTexture* getVideoOutput() override;
    
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
    
    // Called after deserialization to load video
    void deserializeComplete() override;
    
    // Serialization
    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;
    
private:
    ofxHapPlayer player;
    
    // Parameters
    ofParameter<std::string> videoPath;
    ofParameter<bool> loop;
    ofParameter<float> speed;
    ofParameter<bool> playOnLoad;
};
