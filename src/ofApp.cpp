#include "ofApp.h"
#include "core/Registry.h"
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
    
    // 2. Initial load from entry script
    if (!config.entryScript.empty() && ofFile::doesFileExist(config.entryScript)) {
        scriptBridge.runScript(config.entryScript);
    } else {
        ofLogWarning("ofApp") << "No entry script configured. Set entryScript in config.json.";
    }
    
    // 3. Initialize background file watcher
    fileWatcher.watch(ofToDataPath("config.json"));
    fileWatcher.watch(ofToDataPath("scripts"), true); // Watch scripts directory recursively
    fileWatcher.start(500); // Poll background thread every 500ms
    
    graphUI.setup();
}

void ofApp::checkLiveReload() {
    auto changed = fileWatcher.getChangedFiles();
    if (changed.empty()) return;
    
    const auto& config = ConfigManager::get().getConfig();
    
    bool configChanged = false;
    bool scriptsChanged = false;
    bool jsonChanged = false;
    
    for (const auto& path : changed) {
        std::string absPath = path; // fileWatcher already returns absolute paths
        if (absPath == ofToDataPath("config.json")) {
            configChanged = true;
        } else if (absPath == ofToDataPath("scripts/main.json")) {
            jsonChanged = true;
        } else if (ofFilePath::getFileExt(absPath) == "lua") {
            scriptsChanged = true;
        }
    }
    
    if (configChanged) {
        ofLogNotice("ofApp") << "Reloading config...";
        std::string oldEntryScript = config.entryScript;
        ConfigManager::get().load("config.json");
        graphUI.loadConfig();
        const auto& newConfig = ConfigManager::get().getConfig();
        
        // If entryScript changed, reload it
        if (newConfig.entryScript != oldEntryScript && !newConfig.entryScript.empty()) {
            ofLogNotice("ofApp") << "Entry script changed, loading: " << newConfig.entryScript;
            scriptBridge.runScript(newConfig.entryScript);
        }
    } else if (scriptsChanged) {
        ofLogNotice("ofApp") << "Live-reloading: " << config.entryScript;
        scriptBridge.runScript(config.entryScript);
    } else if (jsonChanged) {
        ofLogNotice("ofApp") << "Live-reloading JSON: scripts/main.json";
        session.load("scripts/main.json");
    }
}

void ofApp::update(){
    session.update(ofGetLastFrameTime());
    checkLiveReload();
}

void ofApp::draw(){
    ofBackground(20);
    session.draw();
    
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
    if (key == 'r' && ofGetKeyPressed(OF_KEY_COMMAND)) {
        ofLogNotice("ofApp") << "Reloading config...";
        std::string oldEntryScript = ConfigManager::get().getConfig().entryScript;
        ConfigManager::get().load("config.json");
        graphUI.loadConfig();
        const auto& config = ConfigManager::get().getConfig();
        if (config.entryScript != oldEntryScript && !config.entryScript.empty()) {
            ofLogNotice("ofApp") << "Loading new entry script: " << config.entryScript;
            scriptBridge.runScript(config.entryScript);
        }
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
}

void ofApp::dragEvent(ofDragInfo dragInfo){
}

void ofApp::exit() {
}
