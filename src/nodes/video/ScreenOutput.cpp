#include "ScreenOutput.h"
#include "../../core/Graph.h"

ScreenOutput::ScreenOutput() {
    type = "ScreenOutput";
    
    parameters.add(enabled.set("enabled", true));
    parameters.add(x.set("x", 0, -ofGetWidth(), ofGetWidth()));
    parameters.add(y.set("y", 0, -ofGetHeight(), ofGetHeight()));
    parameters.add(width.set("width", ofGetWidth(), 1, 4096));
    parameters.add(height.set("height", ofGetHeight(), 1, 4096));
}

void ScreenOutput::setup(float posX, float posY, float w, float h) {
    x = posX;
    y = posY;
    width = w;
    height = h;
}

void ScreenOutput::update(float dt) {
    if (!enabled) return;
    
    // Pull input texture from connected node (already updated by graph pull)
    auto inputs = graph->getInputConnections(nodeIndex);
    if (!inputs.empty()) {
        Node* sourceNode = graph->getNode(inputs[0].fromNode);
        if (sourceNode) {
            inputTexture = sourceNode->getVideoOutput();
        }
    }
}

void ScreenOutput::draw() {
    if (!enabled || !inputTexture || !inputTexture->isAllocated()) {
        return;
    }
    
    ofSetColor(255);
    inputTexture->draw(x, y, width, height);
}
