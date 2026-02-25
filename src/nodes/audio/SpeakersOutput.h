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
    
    // Callback for ofSoundStream (OpenFrameworks system callback)
    // Note: No 'override' here because this is an OF-specific callback, 
    // not part of the Crumble Node interface.
    void audioOut(ofSoundBuffer& buffer) {
        // The sound stream always pulls from our connected input (logical output 0 of source)
        pullAudio(buffer, 0);
    }
    
    // Crumble Graph API override
    void pullAudio(ofSoundBuffer& buffer, int index) override {
        if (!graph) {
            buffer.set(0);
            return;
        }
        
        auto inputs = graph->getInputConnections(nodeId);
        if (inputs.empty()) {
            buffer.set(0);
            return;
        }
        
        Node* sourceNode = graph->getNode(inputs[0].fromNode);
        if (!sourceNode) {
            buffer.set(0);
            return;
        }
        
        sourceNode->pullAudio(buffer, inputs[0].fromOutput);
        
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
        ofLogNotice("SpeakersOutput") << "Initializing sound stream: " << sampleRate << "Hz, " << bufferSize << " samples";
        soundStream.stop();
        soundStream.close();
        
        ofSoundStreamSettings settings;
        settings.setOutListener(this); // This node handles audioOut
        settings.sampleRate = sampleRate;
        settings.bufferSize = bufferSize;
        settings.numOutputChannels = 2;
        settings.numInputChannels = 0;
        
        bool success = soundStream.setup(settings);
        if (success) {
            ofLogNotice("SpeakersOutput") << "Sound stream started successfully.";
        } else {
            ofLogError("SpeakersOutput") << "FAILED to start sound stream.";
        }
    }
    
    void onSettingsChanged(int& val) {
        initStream();
    }
};
