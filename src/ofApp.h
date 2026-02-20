#pragma once
#include "ofMain.h"
#include "Commands.h"
#include "core/Patch.h"
#include "nodes/video/VideoMixer.h"
#include "nodes/video/ScreenOutput.h"

class ofApp : public ofBaseApp{

public:
    ofApp() {}
    
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;

    void keyPressed(int key) override;
    void windowResized(int w, int h) override;
    void dragEvent(ofDragInfo dragInfo) override;
    
private:
    Patch patch;
    CommandHistory history;

    // UI state only — no graph internals
    int selectedLayer = 0;
    bool showGui = true;
    
    // Cached node pointers for UI — refreshed after load
    VideoMixer* mixer = nullptr;
    ScreenOutput* output = nullptr;
    void refreshUIPointers();
    
    void addTestLayers(int count);
    void printLayerInfo();
};
