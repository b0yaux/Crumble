#include "ofApp.h"
#include "crumble/Registry.h"

void ofApp::setup(){
    ofSetFrameRate(60);
    ofSetVerticalSync(true);
    ofBackground(20);
    
    // 1. Register node capabilities
    crumble::registerNodes(session);
    scriptBridge.setup(&session);
    
    // 2. Initial load (Lua takes priority if it exists)
    if (ofFile::doesFileExist(luaPath)) {
        scriptBridge.runScript(luaPath);
    } else if (ofFile::doesFileExist(jsonPath)) {
        session.load(jsonPath);
    } else {
        initCrumbleMixer();
    }
    
    // 3. Initialize background file watcher
    fileWatcher.watch(ofToDataPath(luaPath));
    fileWatcher.watch(ofToDataPath(jsonPath));
    fileWatcher.start(500); // Poll background thread every 500ms
    
    refreshUIPointers();
    graphUI.setup();
}

void ofApp::checkLiveReload() {
    auto changed = fileWatcher.getChangedFiles();
    if (changed.empty()) return;
    
    // We want to handle Lua first if both changed
    bool luaChanged = false;
    bool jsonChanged = false;
    
    std::string absLua = ofToDataPath(luaPath);
    std::string absJson = ofToDataPath(jsonPath);
    
    for (const auto& path : changed) {
        if (path == absLua) luaChanged = true;
        else if (path == absJson) jsonChanged = true;
    }
    
    if (luaChanged) {
        ofLogNotice("ofApp") << "Live-reloading script: " << luaPath;
        if (scriptBridge.runScript(luaPath)) {
            refreshUIPointers();
        }
    } else if (jsonChanged) {
        ofLogNotice("ofApp") << "Live-reloading JSON: " << jsonPath;
        if (session.load(jsonPath)) {
            refreshUIPointers();
        }
    }
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
        session.connect(source->nodeId, mixer->nodeId, 0, layerIdx);
    }
    return layerIdx;
}

void ofApp::removeVideoLayer(int layerIndex) {
    if (!mixer || layerIndex < 0 || layerIndex >= mixer->getLayerCount()) return;
    if (mixer->getLayerCount() <= 1) return;
    Node* sourceNode = mixer->getLayerSource(layerIndex);
    int sourceNodeIdx = sourceNode ? sourceNode->nodeId : -1;
    session.removeInput(mixer->nodeId, layerIndex);
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
    
    if (showGui) {
        graphUI.draw(session);
    }
}

void ofApp::keyPressed(int key){
    if (key == 'g' || key == 'G') {
        showGui = !showGui;
        graphUI.setVisible(showGui);
    }
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

void ofApp::mousePressed(int x, int y, int button) {
    graphUI.mousePressed(x, y, button);
}

void ofApp::mouseDragged(int x, int y, int button) {
    graphUI.mouseDragged(x, y, button);
}

void ofApp::mouseReleased(int x, int y, int button) {
    graphUI.mouseReleased(x, y, button);
}

void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {
    graphUI.mouseScrolled(x, y, scrollX, scrollY);
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

void ofApp::exit() {
}
