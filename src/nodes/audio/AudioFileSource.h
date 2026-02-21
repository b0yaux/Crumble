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

    // Serialization
    ofJson serialize() const override {
        ofJson j;
        ofSerialize(j, parameters);
        return j;
    }

    void deserialize(const ofJson& json) override {
        ofJson j = json;
        // Handle common nesting patterns in our JSON
        if (j.contains("AudioSource")) {
            j = j["AudioSource"];
        } else if (j.contains("params")) {
            j = j["params"];
        }

        // Migrate from old "audioPath" key to new "path" key
        std::string pathValue;
        if (j.contains("audioPath")) {
            pathValue = getSafeJson<std::string>(j, "audioPath", "");
        } else if (j.contains("path")) {
            pathValue = getSafeJson<std::string>(j, "path", "");
        }
        if (!pathValue.empty()) path.set(pathValue);

        // Map other parameters with loose typing
        if (j.contains("Volume")) volume = getSafeJson<float>(j, "Volume", volume.get());
        if (j.contains("Speed")) speed = getSafeJson<float>(j, "Speed", speed.get());
        if (j.contains("Loop")) loop = getSafeJson<bool>(j, "Loop", loop.get());
        if (j.contains("Playing")) playing = getSafeJson<bool>(j, "Playing", playing.get());

        ofDeserialize(j, parameters);
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
