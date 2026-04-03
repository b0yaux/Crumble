#include "ofApp.h"
#include "core/Registry.h"
#include "core/Config.h"
#include <filesystem>

void ofApp::setCommandLineConfig(const std::string& configPath,
                                  const std::string& scriptOverride,
                                  const std::string& windowTitle,
                                  const std::string& cwd) {
    m_configPath = configPath;
    m_scriptOverride = scriptOverride;
    m_windowTitle = windowTitle;
    m_cwd = cwd;
}

void ofApp::setup(){
    ofSetFrameRate(60);
    ofSetVerticalSync(true);
    ofBackground(0);
    if (!m_windowTitle.empty()) {
        ofSetWindowTitle(m_windowTitle);
    }
    
    ConfigManager::get().load(m_configPath);
    const auto& config = ConfigManager::get().getConfig();
    
    crumble::registerNodes(session);
    session.getInputBindings().setup();
    interpreter.setup(&session);
    
    std::string scriptToRun = m_scriptOverride.empty() 
                              ? config.entryScript 
                              : m_scriptOverride;
    
    std::string absScriptPath;
    if (!scriptToRun.empty()) {
        namespace fs = std::filesystem;
        
        if (ofFilePath::isAbsolute(scriptToRun)) {
            absScriptPath = scriptToRun;
        } else {
            fs::path cwdPath = fs::path(m_cwd) / scriptToRun;
            if (fs::exists(cwdPath)) {
                absScriptPath = cwdPath.string();
            } else {
                fs::path dataPath = ofToDataPath(scriptToRun);
                if (fs::exists(dataPath)) {
                    absScriptPath = dataPath.string();
                }
            }
        }
        
        if (!absScriptPath.empty() && ofFile::doesFileExist(absScriptPath)) {
            m_activeScriptPath = absScriptPath;
            std::string scriptDir = ofFilePath::getEnclosingDirectory(absScriptPath);
            interpreter.addScriptPath(scriptDir);
            interpreter.runScript(absScriptPath);
            
            fileWatcher.watch(scriptDir, true);
        } else {
            ofLogWarning("ofApp") << "Script not found: " << scriptToRun;
        }
    } else {
        ofLogWarning("ofApp") << "No entry script configured. Set entryScript in config.json or use -s flag.";
    }
    
    fileWatcher.watch(m_configPath);
    fileWatcher.start(500);
    
    graphUI.setup();

    // Start audio engine AFTER nodes and assets have been initialized
    // to avoid concurrent RtAudio initialization cracks.
    session.setupAudio(44100, 256);
}

void ofApp::checkLiveReload() {
    auto changed = fileWatcher.getChangedFiles();
    if (changed.empty()) return;
    
    const auto& config = ConfigManager::get().getConfig();
    
    bool configChanged = false;
    bool scriptsChanged = false;
    
    for (const auto& path : changed) {
        std::string absPath = path; // fileWatcher already returns absolute paths
        if (absPath == ofToDataPath("config.json")) {
            configChanged = true;
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
            session.getGraph().clear();
            
            // Resolve path and update active script
            std::string absPath = ofFilePath::isAbsolute(newConfig.entryScript)
                                  ? newConfig.entryScript
                                  : ofToDataPath(newConfig.entryScript);
            m_activeScriptPath = absPath;
            interpreter.runScript(absPath);
        }
    } else if (scriptsChanged) {
        ofLogNotice("ofApp") << "Live-reloading: " << m_activeScriptPath;
        interpreter.runScript(m_activeScriptPath);
    }
}

void ofApp::update(){
    session.getInputBindings().update();
    session.update(ofGetLastFrameTime());
    interpreter.update(session.getTransport());
    checkLiveReload();
}

void ofApp::draw(){
    ofBackground(0);
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
    if (key == 's' && ofGetKeyPressed(OF_KEY_COMMAND)) session.save("main.json");
    if (key == 'r' && ofGetKeyPressed(OF_KEY_COMMAND)) {
        ofLogNotice("ofApp") << "Reloading config...";
        std::string oldEntryScript = ConfigManager::get().getConfig().entryScript;
        ConfigManager::get().load("config.json");
        graphUI.loadConfig();
        const auto& config = ConfigManager::get().getConfig();
        if (config.entryScript != oldEntryScript && !config.entryScript.empty()) {
            ofLogNotice("ofApp") << "Loading new entry script: " << config.entryScript;
            interpreter.runScript(config.entryScript);
        }
    }
}

void ofApp::mouseMoved(int x, int y) {
    graphUI.mouseMoved(x, y);
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
    session.getInputBindings().cleanup();
}
