#include "SpeakersOutput.h"
#include "../core/Graph.h"

SpeakersOutput::SpeakersOutput() {
    type = "SpeakersOutput";

    parameters.add(masterVolume.set("volume", 1.0, 0.0, 1.0));
    parameters.add(sampleRate.set("sampleRate", 44100, 22050, 96000));
    parameters.add(bufferSize.set("bufferSize", 512, 64, 2048));

    sampleRate.addListener(this, &SpeakersOutput::onSettingsChanged);
    bufferSize.addListener(this, &SpeakersOutput::onSettingsChanged);
}

SpeakersOutput::~SpeakersOutput() {
    soundStream.stop();
    soundStream.close();
}

void SpeakersOutput::setup(int rate, int size) {
    sampleRate.setWithoutEventNotifications(rate);
    bufferSize.setWithoutEventNotifications(size);
    initStream();
}

void SpeakersOutput::onInputConnected(int& toInput) {
    // Automatically start the audio stream when a source is connected
    initStream();
}

void SpeakersOutput::audioOut(ofSoundBuffer& buffer) {
    pullAudio(buffer, 0);
}

void SpeakersOutput::pullAudio(ofSoundBuffer& buffer, int index) {
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

void SpeakersOutput::initStream() {
    ofLogNotice("SpeakersOutput") << "Initializing sound stream: " << sampleRate << "Hz, " << bufferSize << " samples";
    soundStream.stop();
    soundStream.close();

    ofSoundStreamSettings settings;
    settings.setOutListener(this);
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

void SpeakersOutput::onSettingsChanged(int& val) {
    initStream();
}
