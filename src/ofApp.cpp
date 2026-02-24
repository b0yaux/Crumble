#include "ofApp.h"
#include "crumble/Registry.h"
#include "core/Config.h"

void ofApp::setup(){
    ofSetFrameRate(60);
    ofSetVerticalSync(true);
    ofBackground(20);
    
    // 0. Load config
    ConfigManager::get().load("config.json");
    const auto& config = ConfigManager::get().getConfig();
    
    // 1. Register node capabilities
    crumble::registerNodes(session);
    scriptBridge.setup(&session);
    
    // 2. Initial load - use config scripts list, fallback to single path
    if (!config.entryScripts.empty()) {
        scriptBridge.runScripts(config.entryScripts);
    } else if (ofFile::doesFileExist(config.defaultLuaPath)) {
        scriptBridge.runScript(config.defaultLuaPath);
    } else if (ofFile::doesFileExist(config.defaultJsonPath)) {
        session.load(config.defaultJsonPath);
    } else {
        initCrumbleMixer();
    }
    
    // 3. Initialize background file watcher
    for (const auto& script : config.entryScripts) {
        fileWatcher.watch(ofToDataPath(script));
    }
    fileWatcher.watch(ofToDataPath(config.defaultJsonPath));
    fileWatcher.start(500); // Poll background thread every 500ms
    
    refreshUIPointers();
    graphUI.setup();
}

void ofApp::checkLiveReload() {
    auto changed = fileWatcher.getChangedFiles();
    if (changed.empty()) return;
    
    const auto& config = ConfigManager::get().getConfig();
    
    bool scriptsChanged = false;
    bool jsonChanged = false;
    
    for (const auto& path : changed) {
        std::string absPath = ofToDataPath(path);
        if (absPath == ofToDataPath(config.defaultJsonPath)) {
            jsonChanged = true;
        } else {
            for (const auto& script : config.entryScripts) {
                if (absPath == ofToDataPath(script)) {
                    scriptsChanged = true;
                    break;
                }
            }
        }
    }
    
    if (scriptsChanged) {
        ofLogNotice("ofApp") << "Live-reloading scripts...";
        scriptBridge.runScripts(config.entryScripts);
        refreshUIPointers();
    } else if (jsonChanged) {
        ofLogNotice("ofApp") << "Live-reloading JSON: " << config.defaultJsonPath;
        if (session.load(config.defaultJsonPath)) {
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
    if (key == 's' && ofGetKeyPressed(OF_KEY_COMMAND)) session.save("scripts/main.json");
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
            addVideoLayer(file);
            added = true;
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
