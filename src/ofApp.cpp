#include "ofApp.h"
#include "Commands.h"
#include "ops/PatchOps.h"

void ofApp::setup(){
    ofSetFrameRate(60);
    ofSetVerticalSync(true);
    ofBackground(20);
    
    crumble::initDefaultPatch(patch, 1920, 1080);
    refreshUIPointers();
    
    // Try to load default patch if it exists
    if (ofFile::doesFileExist("patches/default.json")) {
        if (patch.load("patches/default.json")) {
            refreshUIPointers();
        }
    }
}

void ofApp::addTestLayers(int count) {
    for (int i = 0; i < count; i++) {
        crumble::addVideoLayer(patch, "");
    }
    refreshUIPointers();
}

void ofApp::printLayerInfo() {
    if (!mixer) return;
    ofLogVerbose("ofApp") << "=== Layer Info ===";
    ofLogVerbose("ofApp") << "Total layers: " << mixer->getLayerCount();
    ofLogVerbose("ofApp") << "Max supported: " << mixer->getMaxSupportedLayers();
    ofLogVerbose("ofApp") << "Connected: " << mixer->getConnectedLayerCount();
    
    for (int i = 0; i < mixer->getLayerCount(); i++) {
        string status = mixer->isLayerConnected(i) ? "CONN" : "----";
        string active = mixer->isLayerActive(i) ? "ON" : "OFF";
        float opacity = mixer->getLayerOpacity(i);
        int blend = mixer->getLayerBlendMode(i);
        
        ofLogVerbose("ofApp") << "Layer " << i << ": " << status << " | " << active 
                   << " | opacity=" << opacity << " | blend=" << blend;
    }
}

void ofApp::update(){
    float dt = ofGetLastFrameTime();
    patch.update(dt);
}

void ofApp::draw(){
    ofBackground(20);
    
    // Draw the graph output
    if (output) {
        output->draw();
    }
    
    // Draw info
    if (showGui && mixer) {
        ofSetColor(255);
        string info = "Crumble - Dynamic Layers\n\n";
        info += "Layers: " + ofToString(mixer->getLayerCount()) + "/" + ofToString(mixer->getMaxSupportedLayers()) + "\n";
        info += "Connected: " + ofToString(mixer->getConnectedLayerCount()) + "\n";
        info += "Selected: Layer " + ofToString(selectedLayer) + "\n\n";
        
        // Show first 8 layers status
        int count = mixer->getLayerCount();
        int displayLayers = count < 8 ? count : 8;
        for (int i = 0; i < displayLayers; i++) {
            string sourceName = mixer->getLayerSourceName(i);
            if (sourceName.length() > 12) {
                sourceName = sourceName.substr(0, 9) + "...";
            }
            string active = mixer->isLayerActive(i) ? "ON" : "OFF";
            float opacity = mixer->getLayerOpacity(i);
            int blend = mixer->getLayerBlendMode(i);
            string blendName;
            if (blend == 0) blendName = "A";
            else if (blend == 1) blendName = "+";
            else if (blend == 2) blendName = "*";
            else blendName = "?";
            
            string sel = (i == selectedLayer) ? ">" : " ";
            info += sel + "L" + ofToString(i + 1) + ": " + sourceName + " | " + active + " | " + ofToString(opacity, 1) + " | " + blendName + "\n";
        }
        
        if (mixer->getLayerCount() > 8) {
            info += "... and " + ofToString(mixer->getLayerCount() - 8) + " more\n";
        }
        
        info += "\nControls:\n";
        info += "+/- : Add/Remove layers\n";
        info += "1-8 : Select & toggle layer\n";
        info += "S   : Select layer\n";
        info += "[/] : Adjust selected opacity\n";
        info += "B   : Cycle selected blend\n";
        info += "G   : Toggle GUI\n";
        info += "Cmd+S : Save (patches/default.json)\n";
        info += "Cmd+L : Load (patches/default.json)\n";
        info += "Cmd+Z : Undo\n";
        info += "Cmd+Shift+Z : Redo\n";
        info += "\nDrag .mov files to load";
        
        ofDrawBitmapString(info, 10, 20);
    }
}

void ofApp::exit(){
}

void ofApp::keyPressed(int key){
    if (key == 'g' || key == 'G') {
        showGui = !showGui;
    }
    
    // Add/remove layers
    if (key == '+' || key == '=') {
        addTestLayers(1);
    }
    if (key == '-' || key == '_') {
        if (mixer && mixer->getLayerCount() > 1) {
            crumble::removeLayer(patch, selectedLayer);
            refreshUIPointers();
            if (selectedLayer >= mixer->getLayerCount()) {
                selectedLayer = mixer->getLayerCount() - 1;
            }
        }
    }
    
    // Layer selection (guard against Cmd+S triggering this too)
    if ((key == 's' || key == 'S') && !ofGetKeyPressed(OF_KEY_COMMAND)) {
        if (mixer) {
            selectedLayer = (selectedLayer + 1) % max(1, mixer->getLayerCount());
        }
    }
    
    // Layer toggles (1-8) — direct node access
    if (key >= '1' && key <= '8') {
        int layer = key - '1';
        if (mixer && layer < mixer->getLayerCount()) {
            selectedLayer = layer;
            mixer->setLayerActive(layer, !mixer->isLayerActive(layer));
        }
    }
    
    // Opacity control — direct node access
    if (key == ']' || key == '}') {
        if (mixer) {
            float op = mixer->getLayerOpacity(selectedLayer);
            mixer->setLayerOpacity(selectedLayer, ofClamp(op + 0.1, 0, 1));
        }
    }
    if (key == '[' || key == '{') {
        if (mixer) {
            float op = mixer->getLayerOpacity(selectedLayer);
            mixer->setLayerOpacity(selectedLayer, ofClamp(op - 0.1, 0, 1));
        }
    }
    
    // Blend mode cycle — direct node access
    if (key == 'b' || key == 'B') {
        if (mixer) {
            int mode = mixer->getLayerBlendMode(selectedLayer);
            mode = (mode + 1) % (int)BlendMode::COUNT;
            mixer->setLayerBlendMode(selectedLayer, (BlendMode)mode);
        }
    }
    
    // Print info
    if (key == 'i' || key == 'I') {
        printLayerInfo();
    }
    
    // Quick add multiple layers for testing
    if (key == 't' || key == 'T') {
        addTestLayers(10);
    }
    
    // Save/Load patches
    if (key == 's' && ofGetKeyPressed(OF_KEY_COMMAND)) {
        std::string path = "patches/default.json";
        if (patch.save(path)) {
            ofLogNotice("ofApp") << "Saved patch to: " << path;
        } else {
            ofLogError("ofApp") << "Failed to save patch";
        }
    }
    if (key == 'l' && ofGetKeyPressed(OF_KEY_COMMAND)) {
        std::string path = "patches/default.json";
        if (ofFile::doesFileExist(path)) {
            cmd::LoadPatch loadCmd;
            loadCmd.filePath = path;
            history.execute(loadCmd, patch.getGraph());
            refreshUIPointers();
        }
    }
    
    // Undo/Redo
    if (key == 'z' && ofGetKeyPressed(OF_KEY_COMMAND)) {
        if (ofGetKeyPressed(OF_KEY_SHIFT)) {
            history.redo(patch.getGraph());
        } else {
            history.undo(patch.getGraph());
        }
        refreshUIPointers();
    }
}

void ofApp::windowResized(int w, int h){
    if (output) {
        output->setup(0, 0, w, h);
    }
}

void ofApp::dragEvent(ofDragInfo dragInfo){
    for (const auto& file : dragInfo.files) {
        // Check actual file extension, not substring
        std::string ext = ofFilePath::getFileExt(file);
        if (ext == "mov" || ext == "hap" || ext == "mp4" || ext == "avi") {
            crumble::addVideoLayer(patch, file);
            refreshUIPointers();
        }
    }
}

void ofApp::refreshUIPointers() {
    mixer = patch.findFirstNodeOfType<VideoMixer>("VideoMixer");
    output = patch.findFirstNodeOfType<ScreenOutput>("ScreenOutput");
}
