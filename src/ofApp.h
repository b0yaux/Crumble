#pragma once
#include "ofMain.h"
#include "core/Session.h"
#include "core/ScriptBridge.h"
#include "nodes/video/VideoMixer.h"
#include "nodes/video/ScreenOutput.h"
#include "nodes/video/VideoFileSource.h"
#include "ui/GraphUI.h"
#include "core/FileWatcher.h"

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void mousePressed(int x, int y, int button);
		void mouseDragged(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseScrolled(int x, int y, float scrollX, float scrollY);
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
    FileWatcher fileWatcher;
    
    // Live-reload paths
    const std::string luaPath = "scripts/main.lua";
    const std::string jsonPath = "scripts/main.json";

    // UI state
    int selectedLayer = 0;
    bool showGui = true;
    GraphUI graphUI;
    
    // Cached node pointers for UI
    VideoMixer* mixer = nullptr;
    ScreenOutput* output = nullptr;
    
    void printLayerInfo();
};
