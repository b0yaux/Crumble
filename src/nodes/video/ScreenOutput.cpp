#include "ScreenOutput.h"
#include "../../core/Graph.h"

ScreenOutput::ScreenOutput() {
    type = "ScreenOutput";
    
    parameters.add(enabled.set("enabled", true));
    
    // Use hardcoded defaults if ofGetWidth is not yet valid (0)
    float defaultW = ofGetWidth() > 0 ? ofGetWidth() : 1280;
    float defaultH = ofGetHeight() > 0 ? ofGetHeight() : 720;
    
    parameters.add(x.set("x", 0, -4096, 4096));
    parameters.add(y.set("y", 0, -4096, 4096));
    parameters.add(width.set("width", defaultW, 1, 4096));
    parameters.add(height.set("height", defaultH, 1, 4096));
}

void ScreenOutput::setup(float posX, float posY, float w, float h) {
    x = posX;
    y = posY;
    width = w;
    height = h;
}

void ScreenOutput::update(float dt) {
    inputTexture = nullptr; // Reset every frame
    if (!enabled) return;
    
    if (!graph) return;
    
    // Pull input texture from connected node (already updated by graph pull)
    auto inputs = graph->getInputConnections(nodeId);
    if (!inputs.empty()) {
        Node* sourceNode = graph->getNode(inputs[0].fromNode);
        if (sourceNode) {
            inputTexture = sourceNode->getVideoOutput();
        }
    }
}

void ScreenOutput::draw() {
    if (!enabled) return;
    
    if (!inputTexture || !inputTexture->isAllocated()) {
        // Draw a dark gray background to indicate the node is active but has no input
        ofSetColor(40);
        ofDrawRectangle(x, y, width, height);
        
        // Optionally draw an indicator
        ofSetColor(200);
        ofDrawBitmapString("No Input", x + width/2 - 30, y + height/2);
        return;
    }
    
    ofSetColor(255);
    inputTexture->draw(x, y, width, height);
}
