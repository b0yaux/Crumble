#include "ofApp.h"

void ofApp::setup(){
    ofSetFrameRate(60);
    ofSetVerticalSync(true);
    ofBackground(20);
    ofSetLogLevel(OF_LOG_VERBOSE);

    ofLogNotice("ofApp") << "=== Crumble Dynamic Layers Setup ===";

    mixer = &mainGraph.addNode<VideoMixer>();
    mixer->name = "Mixer";
    mixer->setup(1920, 1080);
    mixer->setLayerCount(1);
    
    ofLogNotice("ofApp") << "Mixer supports up to " << mixer->getMaxSupportedLayers() << " layers";

    output = &mainGraph.addNode<ScreenOutput>();
    output->name = "Output";
    output->setup(0, 0, ofGetWidth(), ofGetHeight());

    mainGraph.connect(0, 1);
    mainGraph.setVideoOutputNode(0);
    
    ofLogNotice("ofApp") << "Setup complete - drag .mov files to add layers";
    ofLogNotice("ofApp") << "Current layers: " << (mixer ? mixer->getLayerCount() : 0);
}

void ofApp::addVideoLayer(const std::string& filePath) {
    // Create new source and get its index IMMEDIATELY
    auto* source = &mainGraph.addNode<VideoFileSource>();
    int sourceIdx = mainGraph.getNodeCount() - 1;  // This is the correct index!
    
    source->name = "Video_" + ofToString(allSources.size());
    source->load(filePath);
    source->play();
    
    allSources.push_back(source);
    
    // First, try to find an empty slot in existing layers
    int layerIdx = -1;
    for (int i = 0; i < mixer->getLayerCount(); i++) {
        if (!mixer->isLayerConnected(i)) {
            layerIdx = i;
            break;
        }
    }
    
    // If no empty slot, add a new layer
    if (layerIdx < 0) {
        layerIdx = mixer->addLayer(source);
    } else {
        // Fill existing empty slot
        mixer->setLayerSource(layerIdx, source);
        ofLogNotice("ofApp") << "Filled empty slot at layer " << layerIdx;
    }
    
    if (layerIdx >= 0) {
        // Connect source to mixer at the layer index
        mainGraph.connect(sourceIdx, 0, 0, layerIdx);
        
        ofLogNotice("ofApp") << "Connected node " << sourceIdx << " -> mixer layer " << layerIdx;
        ofLogNotice("ofApp") << "Loaded video into layer " << layerIdx << ": " << filePath;
    }
}

void ofApp::addTestLayers(int count) {
    // Create test video sources (just placeholders that will show black)
    for (int i = 0; i < count; i++) {
        // Create source and get its index IMMEDIATELY (before any other operations)
        auto* source = &mainGraph.addNode<VideoFileSource>();
        int sourceIdx = mainGraph.getNodeCount() - 1;  // This is the correct index!
        
        source->name = "Video_" + ofToString(allSources.size());
        allSources.push_back(source);
        
        int layerIdx = mixer->addLayer(source);
        if (layerIdx >= 0) {
            // Connect: source node index -> mixer node index (0) -> mixer layer index
            mainGraph.connect(sourceIdx, 0, 0, layerIdx);
            
            // Set varying opacity for visibility
            mixer->setLayerOpacity(layerIdx, 1.0f);
            
            ofLogNotice("ofApp") << "Test layer: connected node " << sourceIdx << " -> mixer layer " << layerIdx;
        }
    }
    
    ofLogNotice("ofApp") << "Added " << count << " test layers (total: " << mixer->getLayerCount() << ")";
}

void ofApp::removeLastLayer() {
    int currentCount = mixer->getLayerCount();
    if (currentCount > 1) {
        mixer->removeLayer(currentCount - 1);
        
        // Note: In a real implementation we'd disconnect and remove the source node too
        ofLogNotice("ofApp") << "Removed last layer (total: " << mixer->getLayerCount() << ")";
    }
}

void ofApp::printLayerInfo() {
    ofLogNotice("ofApp") << "=== Layer Info ===";
    ofLogNotice("ofApp") << "Total layers: " << mixer->getLayerCount();
    ofLogNotice("ofApp") << "Max supported: " << mixer->getMaxSupportedLayers();
    ofLogNotice("ofApp") << "Connected: " << mixer->getConnectedLayerCount();
    
    for (int i = 0; i < mixer->getLayerCount(); i++) {
        string status = mixer->isLayerConnected(i) ? "CONN" : "----";
        string active = mixer->isLayerActive(i) ? "ON" : "OFF";
        float opacity = mixer->getLayerOpacity(i);
        int blend = mixer->getLayerBlendMode(i);
        
        ofLogNotice("ofApp") << "Layer " << i << ": " << status << " | " << active 
                   << " | opacity=" << opacity << " | blend=" << blend;
    }
}

void ofApp::update(){
    float dt = ofGetLastFrameTime();
    mainGraph.update(dt);
}

void ofApp::draw(){
    ofBackground(20);
    
    // Draw the graph output
    output->draw();
    
    // Draw info
    if (showGui) {
        ofSetColor(255);
        string info = "Crumble - Dynamic Layers\n\n";
        info += "Layers: " + ofToString(mixer->getLayerCount()) + "/" + ofToString(mixer->getMaxSupportedLayers()) + "\n";
        info += "Connected: " + ofToString(mixer->getConnectedLayerCount()) + "\n";
        info += "Selected: Layer " + ofToString(selectedLayer) + "\n\n";
        
        // Show first 8 layers status
        int displayLayers = mixer->getLayerCount() < 8 ? mixer->getLayerCount() : 8;
        for (int i = 0; i < displayLayers; i++) {
            string status = mixer->isLayerConnected(i) ? "OK" : "--";
            string active = mixer->isLayerActive(i) ? "ON" : "OFF";
            float opacity = mixer->getLayerOpacity(i);
            int blend = mixer->getLayerBlendMode(i);
            string blendName;
            if (blend == 0) blendName = "ALPHA";
            else if (blend == 1) blendName = "ADD";
            else if (blend == 2) blendName = "MULT";
            else blendName = "???";
            
            string sel = (i == selectedLayer) ? "*" : " ";
            info += sel + "L" + ofToString(i+1) + ": " + status + " | " + active + " | " + ofToString(opacity, 1) + " | " + blendName + "\n";
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
        removeLastLayer();
    }
    
    // Layer selection
    if (key == 's' || key == 'S') {
        selectedLayer = (selectedLayer + 1) % max(1, mixer->getLayerCount());
        ofLogNotice("ofApp") << "Selected layer: " << selectedLayer;
    }
    
    // Layer toggles (1-8)
    if (key >= '1' && key <= '8') {
        int layer = key - '1';
        if (layer < mixer->getLayerCount()) {
            selectedLayer = layer;
            mixer->setLayerActive(layer, !mixer->isLayerActive(layer));
        }
    }
    
    // Opacity control for selected layer
    if (key == ']' || key == '}') {
        float op = mixer->getLayerOpacity(selectedLayer);
        mixer->setLayerOpacity(selectedLayer, ofClamp(op + 0.1, 0, 1));
    }
    if (key == '[' || key == '{') {
        float op = mixer->getLayerOpacity(selectedLayer);
        mixer->setLayerOpacity(selectedLayer, ofClamp(op - 0.1, 0, 1));
    }
    
    // Blend mode cycle
    if (key == 'b' || key == 'B') {
        int mode = mixer->getLayerBlendMode(selectedLayer);
        mode = (mode + 1) % (int)BlendMode::COUNT;
        mixer->setLayerBlendMode(selectedLayer, (BlendMode)mode);
    }
    
    // Print info
    if (key == 'i' || key == 'I') {
        printLayerInfo();
    }
    
    // Quick add multiple layers for testing
    if (key == 't' || key == 'T') {
        // Add 10 test layers
        addTestLayers(10);
    }
}

void ofApp::windowResized(int w, int h){
    if (output) {
        output->setup(0, 0, w, h);
    }
}

void ofApp::dragEvent(ofDragInfo dragInfo){
    for (const auto& file : dragInfo.files) {
        if (ofIsStringInString(file, ".mov") || ofIsStringInString(file, ".hap")) {
            addVideoLayer(file);
        }
    }
}
