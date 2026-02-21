#pragma once
#include "../../core/Node.h"
#include "../../core/Session.h"
#include "ofxAudioFile.h"

/**
 * AudioFileSource node using ofxAudioFile via AssetPool.
 * Decouples RAM buffers from the node lifecycle.
 */
class AudioFileSource : public Node {
public:
    AudioFileSource() {
        type = "AudioFileSource";

        parameters.setName("AudioSource");
        parameters.add(path.set("path", ""));
        parameters.add(volume.set("Volume", 1.0, 0.0, 1.0));
        parameters.add(speed.set("Speed", 1.0, -4.0, 4.0));
        parameters.add(loop.set("Loop", true));
        parameters.add(playing.set("Playing", true));

        path.addListener(this, &AudioFileSource::onPathChanged);
    }
    
    void audioOut(ofSoundBuffer& buffer) override {
        if (!playing || !sharedLoader || !sharedLoader->loaded() || sharedLoader->length() == 0) {
            return;
        }

        float* data = sharedLoader->data();
        size_t totalSamples = sharedLoader->length();
        int channels = sharedLoader->channels();
        
        for (size_t i = 0; i < buffer.getNumFrames(); i++) {
            size_t frameIndex = (size_t)playhead;
            
            if (frameIndex < totalSamples) {
                for (int c = 0; c < buffer.getNumChannels(); c++) {
                    int sourceChannel = c % channels;
                    float sample = data[frameIndex * channels + sourceChannel];
                    buffer[i * buffer.getNumChannels() + c] += sample * (float)volume;
                }
            }
            
            playhead += (float)speed;
            
            if (playhead >= (float)totalSamples) {
                if (loop) {
                    playhead = fmod(playhead, (float)totalSamples);
                } else {
                    playing = false;
                    playhead = 0;
                    break;
                }
            } else if (playhead < 0) {
                if (loop) {
                    playhead = (float)totalSamples + fmod(playhead, (float)totalSamples);
                } else {
                    playing = false;
                    playhead = 0;
                    break;
                }
            }
        }
    }
    
    std::string getDisplayName() const override {
        if (path.get().empty()) return "Empty Audio";
        return ofFilePath::getFileName(path.get());
    }

protected:
    std::shared_ptr<ofxAudioFile> sharedLoader;
    double playhead = 0;
    std::string loadedPath;

    ofParameter<std::string> path;
    ofParameter<float> volume;
    ofParameter<float> speed;
    ofParameter<bool> loop;
    ofParameter<bool> playing;
    
    void onPathChanged(std::string& p) {
        if (p.empty() || p == loadedPath) return;

        if (g_session) {
            sharedLoader = g_session->getAssets().getAudio(p);
            if (sharedLoader) {
                playhead = 0;
                loadedPath = p;
                ofLogNotice("AudioFileSource") << "Got shared audio: " << p;
            } else {
                ofLogError("AudioFileSource") << "FAILED to load: " << p;
            }
        }
    }
};
