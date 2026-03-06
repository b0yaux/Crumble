#pragma once
#include "../core/Node.h"
#include "ofxAudioFile.h"
#include <atomic>

/**
 * AudioFileSource node using ofxAudioFile via AssetCache.
 * Decouples RAM buffers from the node lifecycle.
 */
class AudioFileSource : public Node {
public:
    AudioFileSource();

    void processAudio(ofSoundBuffer& buffer, int index = 0) override;
    void load(const std::string& path);
    std::string getDisplayName() const override;

    // Serialization
    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;
    
    // Get normalized playhead position (0.0 to 1.0)
    double getRelativePosition() const;
    void setRelativePosition(double pct);

protected:
    std::shared_ptr<ofxAudioFile> sharedLoader;
    std::atomic<double> playhead{0.0};
    std::string loadedPath;

    ofParameter<std::string> path;
    ofParameter<float> speed;
    ofParameter<bool> loop;
    ofParameter<bool> playing;

    void onPathChanged(std::string& p);
};
