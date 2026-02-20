#include "ofApp.h"
#include "crumble/Registry.h"

void ofApp::setup(){
    ofSetFrameRate(60);
    ofSetVerticalSync(true);
    ofBackground(20);
    
    // 1. Register node capabilities
    crumble::registerNodes(session);
    
    // 2. Initial load
    std::string path = "scripts/main.json";
    if (ofFile::doesFileExist(path)) {
        session.load(path);
        try {
            lastScriptMod = std::filesystem::last_write_time(ofToDataPath(path));
        } catch (...) {}
    } else {
        initCrumbleMixer();
    }
    
    refreshUIPointers();
}

void ofApp::checkLiveReload() {
    std::string path = "scripts/main.json";
    if (!ofFile::doesFileExist(path)) return;
    
    try {
        auto mtime = std::filesystem::last_write_time(ofToDataPath(path));
        if (mtime > lastScriptMod) {
            lastScriptMod = mtime;
            ofLogNotice("ofApp") << "Live-reloading " << path;
            if (session.load(path)) {
                refreshUIPointers();
            }
        }
    } catch (...) {}
}

void ofApp::initCrumbleMixer() {
    session.clear();
    auto* mixerNode = dynamic_cast<VideoMixer*>(session.addNode("VideoMixer", "Mixer"));
    if (mixerNode) {
        mixerNode->setup(1920, 1080);
        mixerNode->setLayerCount(1);
    }
    auto* outputNode = dynamic_cast<ScreenOutput*>(session.addNode("ScreenOutput", "Output"));
    if (outputNode) {
        outputNode->setup(0, 0, 1920, 1080);
    }
    session.connect(0, 1);
    session.setVideoOutputNode(0);
    refreshUIPointers();
}

int ofApp::addVideoLayer(const std::string& filePath) {
    if (!mixer) return -1;
    auto* source = dynamic_cast<VideoFileSource*>(session.addNode("VideoFileSource"));
    if (!source) return -1;
    if (!filePath.empty()) {
        source->load(filePath);
        source->play();
    }
    int layerIdx = -1;
    for (int i = 0; i < mixer->getLayerCount(); i++) {
        if (!mixer->isLayerConnected(i)) {
            layerIdx = i;
            break;
        }
    }
    if (layerIdx < 0) layerIdx = mixer->addLayer();
    if (layerIdx >= 0) {
        session.connect(source->nodeIndex, mixer->nodeIndex, 0, layerIdx);
    }
    return layerIdx;
}

void ofApp::removeVideoLayer(int layerIndex) {
    if (!mixer || layerIndex < 0 || layerIndex >= mixer->getLayerCount()) return;
    if (mixer->getLayerCount() <= 1) return;
    Node* sourceNode = mixer->getLayerSource(layerIndex);
    int sourceNodeIdx = sourceNode ? sourceNode->nodeIndex : -1;
    session.removeInput(mixer->nodeIndex, layerIndex);
    if (sourceNodeIdx >= 0) session.removeNode(sourceNodeIdx);
    mixer->removeLayer(layerIndex);
}

void ofApp::addTestLayers(int count) {
    session.checkpoint();
    for (int i = 0; i < count; i++) addVideoLayer("");
    refreshUIPointers();
}

void ofApp::update(){
    session.update(ofGetLastFrameTime());
    checkLiveReload();
}

void ofApp::draw(){
    ofBackground(20);
    if (output) output->draw();
    
    if (showGui && mixer) {
        ofSetColor(255);
        string info = "Crumble [JSON Live-Reload Mode]\n\n";
        info += "Nodes: " + ofToString(session.getNodeCount()) + "\n";
        info += "Watching: bin/data/scripts/main.json\n\n";
        
        for (int i = 0; i < std::min(8, mixer->getLayerCount()); i++) {
            string sel = (i == selectedLayer) ? ">" : " ";
            info += sel + "L" + ofToString(i + 1) + ": " + mixer->getLayerSourceName(i) + "\n";
        }
        ofDrawBitmapString(info, 10, 20);
    }
}

void ofApp::keyPressed(int key){
    if (key == 'g' || key == 'G') showGui = !showGui;
    if (key == '+' || key == '=') addTestLayers(1);
    if (key == '-' || key == '_') {
        session.checkpoint();
        removeVideoLayer(selectedLayer);
        refreshUIPointers();
    }
    if (key == 's' && ofGetKeyPressed(OF_KEY_COMMAND)) session.save("scripts/main.json");
    if (key == 'z' && ofGetKeyPressed(OF_KEY_COMMAND)) {
        if (ofGetKeyPressed(OF_KEY_SHIFT)) session.redo();
        else session.undo();
        refreshUIPointers();
    }
}

void ofApp::windowResized(int w, int h){
    if (output) output->setup(0, 0, w, h);
}

void ofApp::dragEvent(ofDragInfo dragInfo){
    bool added = false;
    for (const auto& file : dragInfo.files) {
        std::string ext = ofFilePath::getFileExt(file);
        if (ext == "mov" || ext == "hap" || ext == "mp4" || ext == "avi") {
            if (!added) { session.checkpoint(); added = true; }
            addVideoLayer(file);
        }
    }
    if (added) refreshUIPointers();
}

void ofApp::refreshUIPointers() {
    mixer = session.findFirstNode<VideoMixer>();
    output = session.findFirstNode<ScreenOutput>();
}

void ofApp::exit() {}
