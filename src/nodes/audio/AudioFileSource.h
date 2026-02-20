#pragma once
#include "../../core/Node.h"
#include "ofxAudioFile.h"

/**
 * AudioFileSource node using ofxAudioFile.
 * Loads audio data into RAM for low-latency playback.
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
        if (!playing || !loader.loaded() || loader.length() == 0) {
            return;
        }

        float* data = loader.data();
        size_t totalSamples = loader.length();
        int channels = loader.channels();
        
        for (size_t i = 0; i < buffer.getNumFrames(); i++) {
            size_t frameIndex = (size_t)playhead;
            
            if (frameIndex < totalSamples) {
                for (int c = 0; c < buffer.getNumChannels(); c++) {
                    // Simple channel mapping: wrap if loader has fewer channels
                    int sourceChannel = c % channels;
                    float sample = data[frameIndex * channels + sourceChannel];
                    buffer[i * buffer.getNumChannels() + c] += sample * (float)volume;
                }
            }
            
            // Advance playhead
            playhead += (float)speed;
            
            // Handle looping
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
    ofxAudioFile loader;
    double playhead = 0; // in frames
    
    ofParameter<std::string> audioPath;
    ofParameter<float> volume;
    ofParameter<float> speed;
    ofParameter<bool> loop;
    ofParameter<bool> playing;
    
    void onPathChanged(std::string& path) {
        if (!path.empty()) {
            std::string fullPath = path;
            // If not absolute, use ofToDataPath
            if (path.find("/") != 0 && path.find(":") == std::string::npos) {
                fullPath = ofToDataPath(path);
            }
            
            ofLogNotice("AudioFileSource") << "Attempting to load: " << fullPath;
            loader.load(fullPath);
            playhead = 0;
            if (loader.loaded()) {
                ofLogNotice("AudioFileSource") << "Successfully loaded: " << fullPath << " (" << loader.length() << " frames)";
            } else {
                ofLogError("AudioFileSource") << "FAILED to load: " << fullPath;
            }
        }
    }
};
