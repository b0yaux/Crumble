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
        parameters.add(audioPath.set("audioPath", ""));
        parameters.add(volume.set("Volume", 1.0, 0.0, 1.0));
        parameters.add(speed.set("Speed", 1.0, -4.0, 4.0));
        parameters.add(loop.set("Loop", true));
        parameters.add(playing.set("Playing", true));
        
        audioPath.addListener(this, &AudioFileSource::onPathChanged);
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
        if (audioPath.get().empty()) return "Empty Audio";
        return ofFilePath::getFileName(audioPath.get());
    }

protected:
    std::shared_ptr<ofxAudioFile> sharedLoader;
    double playhead = 0;
    std::string loadedPath;
    
    ofParameter<std::string> audioPath;
    ofParameter<float> volume;
    ofParameter<float> speed;
    ofParameter<bool> loop;
    ofParameter<bool> playing;
    
    void onPathChanged(std::string& path) {
        if (path.empty() || path == loadedPath) return;
        
        if (g_session) {
            sharedLoader = g_session->getAssets().getAudio(path);
            if (sharedLoader) {
                playhead = 0;
                loadedPath = path;
                ofLogNotice("AudioFileSource") << "Got shared audio: " << path;
            } else {
                ofLogError("AudioFileSource") << "FAILED to load: " << path;
            }
        }
    }
};
