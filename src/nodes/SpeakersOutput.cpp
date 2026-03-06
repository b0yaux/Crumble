#include "SpeakersOutput.h"
#include "../core/Graph.h"

SpeakersOutput::SpeakersOutput() {
    type = "SpeakersOutput";
    parameters.add(masterVolume.set("masterVolume", 1.0, 0.0, 1.0));
}

SpeakersOutput::~SpeakersOutput() {
}

void SpeakersOutput::processAudio(ofSoundBuffer& buffer, int index) {
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

    // Apply source volume (including modulation)
    Control sourceVolCtrl = sourceNode->getControl(sourceNode->volume);
    for (size_t i = 0; i < buffer.getNumFrames(); i++) {
        float gain = sourceVolCtrl[i];
        if (gain != 1.0f) {
            for (int c = 0; c < buffer.getNumChannels(); c++) {
                buffer[i * buffer.getNumChannels() + c] *= gain;
            }
        }
    }

    if (masterVolume != 1.0f) {
        buffer *= (float)masterVolume;
    }
}
