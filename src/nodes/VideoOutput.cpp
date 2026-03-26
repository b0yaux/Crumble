#include "VideoOutput.h"
#include "ofMain.h"
#include "../core/NodeProcessor.h"
#include "../core/ProcessorCommand.h"

using namespace crumble;

class VideoOutputProcessor : public VideoProcessor {
public:
    ControlSlot* activeSlot = nullptr;

    VideoOutputProcessor() {
        activeSlot = getControlPtr(crumble::hashString("active"));
    }

    void processVideo(double cycle, double cycleStep) override {
        currentCycle = cycle;
    }

    ofTexture* getOutput(int index = 0) override {
        if (evalSlot(activeSlot, currentCycle) < 0.5f) return nullptr;
        return nullptr; // This node is a sink, it doesn't provide output to others.
    }
};

VideoOutput::VideoOutput() {
    type = "videoout";
    canDraw = true;
    
    parameters->add(autoFullscreen.set("autoFullscreen", true));
    
    // Use hardcoded defaults if ofGetWidth is not yet valid (0)
    float defaultW = ofGetWidth() > 0 ? ofGetWidth() : 1280;
    float defaultH = ofGetHeight() > 0 ? ofGetHeight() : 720;
    
    parameters->add(x.set("x", 0, -4096, 4096));
    parameters->add(y.set("y", 0, -4096, 4096));
    parameters->add(width.set("width", defaultW, 1, 4096));
    parameters->add(height.set("height", defaultH, 1, 4096));
}

void VideoOutput::setupProcessor() {
    Node::setupProcessor();
    if (videoProcessor) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::REGISTER_ENDPOINT;
        pushCommand(cmd);
    }
}

void VideoOutput::onParameterChanged(const std::string& paramName) {
    Node::onParameterChanged(paramName);
}

VideoProcessor* VideoOutput::createVideoProcessor() {
    return new VideoOutputProcessor();
}

void VideoOutput::setup(float posX, float posY, float w, float h) {
    x = posX;
    y = posY;
    width = w;
    height = h;
}

void VideoOutput::update(float dt) {
    if (autoFullscreen) {
        x = 0;
        y = 0;
        width = ofGetWidth();
        height = ofGetHeight();
    }

    inputTexture = nullptr; // Reset every frame
    sourceOpacity = 1.0f;
    if (!active->get()) return;
    
    Node* sourceNode = getInputNode(0);
    
    if (sourceNode) {
        inputTexture = sourceNode->getVideoOutput(0);
        Control sourceOpCtrl = sourceNode->getControl(*sourceNode->opacity);
        sourceOpacity = sourceOpCtrl[0];
    }
}

void VideoOutput::draw() {
    if (!active->get()) return;
    
    if (!inputTexture || !inputTexture->isAllocated()) {
        ofSetColor(40);
        ofDrawRectangle(x, y, width, height);
        ofSetColor(200);
        ofDrawBitmapString("No Input", x + width/2 - 30, y + height/2);
        return;
    }
    
    ofSetColor(255, sourceOpacity * 255);
    inputTexture->draw(x, y, width, height);
}
