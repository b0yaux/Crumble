#pragma once
#include "ofMain.h"
#include "core/Node.h"
#include "core/Graph.h"
#include "nodes/video/VideoFileSource.h"
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
    Graph mainGraph;

    VideoMixer* mixer;
    ScreenOutput* output;
    
    // Track all video sources
    std::vector<VideoFileSource*> allSources;
    
    // Test mode
    int testLayerCount = 2;
    int selectedLayer = 0;
    
    void addVideoLayer(const std::string& filePath);
    void addTestLayers(int count);
    void removeLastLayer();
    void printLayerInfo();

    bool showGui = true;
};
