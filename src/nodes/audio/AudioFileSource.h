#pragma once
#include "../../core/Node.h"
#include "ofxAudioFile.h"

/**
 * AudioFileSource node using ofxAudioFile via AssetPool.
 * Decouples RAM buffers from the node lifecycle.
 */
class AudioFileSource : public Node {
public:
    AudioFileSource();

    void pullAudio(ofSoundBuffer& buffer, int index = 0) override;
    std::string getDisplayName() const override;

    // Serialization
    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;

protected:
    std::shared_ptr<ofxAudioFile> sharedLoader;
    double playhead = 0;
    std::string loadedPath;

    ofParameter<std::string> path;
    ofParameter<float> volume;
    ofParameter<float> speed;
    ofParameter<bool> loop;
    ofParameter<bool> playing;

    void onPathChanged(std::string& p);
};
