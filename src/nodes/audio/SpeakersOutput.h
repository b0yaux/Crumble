#pragma once
#include "core/Node.h"
#include "core/Graph.h"

/**
 * SpeakersOutput acts as the final audio sink.
 * It owns the ofSoundStream and pulls audio from the Graph.
 */
class SpeakersOutput : public Node {
public:
    SpeakersOutput() {
        type = "SpeakersOutput";
        
        parameters.setName("SpeakersOutput");
        parameters.add(masterVolume.set("Volume", 1.0, 0.0, 1.0));
        parameters.add(sampleRate.set("SampleRate", 44100, 22050, 96000));
        parameters.add(bufferSize.set("BufferSize", 512, 64, 2048));
        
        sampleRate.addListener(this, &SpeakersOutput::onSettingsChanged);
        bufferSize.addListener(this, &SpeakersOutput::onSettingsChanged);
    }
    
    virtual ~SpeakersOutput() {
        soundStream.stop();
        soundStream.close();
    }
    
    void setup(int rate = 44100, int size = 512) {
        sampleRate.setWithoutEventNotifications(rate);
        bufferSize.setWithoutEventNotifications(size);
        initStream();
    }
    
    void onInputConnected(int& toInput) override {
        // Automatically start the audio stream when a source is connected
        initStream();
    }
    
    void audioOut(ofSoundBuffer& buffer) override {
        if (!graph) {
            // Diagnostic: Test tone if no graph connected
            // for(size_t i=0; i<buffer.size(); i++) buffer[i] = sin(i*0.1)*0.1; 
            return;
        }
        
        // Pull from the graph's master audio output
        graph->audioOut(buffer);
        
        static int counter = 0;
        if (counter++ % 100 == 0) {
            float rms = buffer.getRMSAmplitude();
            ofLogNotice("SpeakersOutput") << "RMS: " << rms << " BufferSize: " << buffer.getNumFrames();
        }

        // Apply master volume
        if (masterVolume != 1.0f) {
            buffer *= (float)masterVolume;
        }
    }
    
protected:
    ofSoundStream soundStream;
    ofParameter<float> masterVolume;
    ofParameter<int> sampleRate;
    ofParameter<int> bufferSize;
    
    void initStream() {
        soundStream.stop();
        soundStream.close();
        
        ofSoundStreamSettings settings;
        settings.setOutListener(this); // This node handles audioOut
        settings.sampleRate = sampleRate;
        settings.bufferSize = bufferSize;
        settings.numOutputChannels = 2;
        settings.numInputChannels = 0;
        
        soundStream.setup(settings);
    }
    
    void onSettingsChanged(int& val) {
        initStream();
    }
};
