#pragma once
#include "ofMain.h"
#include "core/Session.h"
#include "core/ScriptEngine.h"
#include "nodes/video/VideoMixer.h"
#include "nodes/video/VideoFileSource.h"
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
    
    // Interaction-specific helpers (The "Mixer" workflow)
    void initCrumbleMixer();
    int  addVideoLayer(const std::string& filePath = "");
    void removeVideoLayer(int layerIndex);
    
    void refreshUIPointers();

private:
    Session session;
    ScriptEngine scriptEngine;
    
    // Live-reload state
    std::filesystem::file_time_type lastJsonMod;
    std::filesystem::file_time_type lastLuaMod;
    void checkLiveReload();

    // UI state
    int selectedLayer = 0;
    bool showGui = true;
    
    // Cached node pointers for UI
    VideoMixer* mixer = nullptr;
    ScreenOutput* output = nullptr;
    
    void addTestLayers(int count);
    void printLayerInfo();
};
