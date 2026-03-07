#pragma once
#include "../core/Node.h"
#include "../core/NodeProcessor.h"
#include "ofxAudioFile.h"
#include <atomic>
#include <string>

namespace crumble {
    class NodeProcessor;
}

/**
 * AudioFileSource node using ofxAudioFile via AssetCache.
 * Decouples RAM buffers from the node lifecycle.
 */
class AudioFileSource : public Node {
public:
    AudioFileSource();
    
    // Performance: Members made public to allow AVSampler direct pointer access
    ofParameter<std::string> path;
    ofParameter<float> speed;
    ofParameter<bool> loop;
    ofParameter<bool> playing;

    void processAudio(ofSoundBuffer& buffer, int index = 0) override;
    void load(const std::string& path);
    std::string getDisplayName() const override;

    // Serialization
    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;
    
    // Get normalized playhead position (0.0 to 1.0)
    double getRelativePosition() const;
    void setRelativePosition(double pct);

    crumble::AudioProcessor* createAudioProcessor() override;
    void onParameterChanged(const std::string& paramName) override;

private:
    void onPathChanged(std::string& p);
    std::string loadedPath;
    std::shared_ptr<ofxAudioFile> sharedLoader;
};
