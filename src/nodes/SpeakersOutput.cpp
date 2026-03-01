#include "SpeakersOutput.h"
#include "../core/Graph.h"

SpeakersOutput::SpeakersOutput() {
    type = "SpeakersOutput";
    parameters.add(masterVolume.set("volume", 1.0, 0.0, 1.0));
}

SpeakersOutput::~SpeakersOutput() {
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
