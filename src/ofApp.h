#pragma once
#include "ofMain.h"
#include "core/Session.h"
#include "core/ScriptBridge.h"
#include "nodes/video/VideoMixer.h"
#include "nodes/video/ScreenOutput.h"
#include "nodes/video/VideoFileSource.h"

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void exit();

    // Crumble logic
    void initCrumbleMixer();
    int addVideoLayer(const std::string& filePath);
    void removeVideoLayer(int layerIndex);
    void addTestLayers(int count);
    void refreshUIPointers();
    void checkLiveReload();

    Session session;
    ScriptBridge scriptBridge;
    
    // Live-reload state
    std::filesystem::file_time_type lastJsonMod;
    std::filesystem::file_time_type lastLuaMod;

    // UI state
    int selectedLayer = 0;
    bool showGui = true;
    
    // Cached node pointers for UI
    VideoMixer* mixer = nullptr;
    ScreenOutput* output = nullptr;
    
    void printLayerInfo();
};
