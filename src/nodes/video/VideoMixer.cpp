#include "VideoMixer.h"
#include "../../core/Graph.h"

VideoMixer::VideoMixer() {
    type = "VideoMixer";
    
    // Add numActiveLayers parameter
    parameters.add(numActiveLayers.set("numLayers", 2, 1, 128));
    
    // Initialize with default 2 layers
    resizeLayerArrays(2);
}

VideoMixer::~VideoMixer() {
}

void VideoMixer::detectGpuLimits() {
    GLint maxSamplers = 16;  // OpenGL minimum
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxSamplers);
    
    // Cap at 128 for practical purposes (M1 Pro supports 128)
    maxSupportedLayers = std::min(128, (int)maxSamplers);
    
    ofLogNotice("VideoMixer") << "GPU detected: " << maxSupportedLayers << " texture samplers available";
    
    // Update parameter range
    numActiveLayers.setMax(maxSupportedLayers);
}

void VideoMixer::setup(int width, int height) {
    fboWidth = width;
    fboHeight = height;
    
    detectGpuLimits();
    allocateFbo();
}

void VideoMixer::allocateFbo() {
    ofFboSettings settings;
    settings.width = fboWidth;
    settings.height = fboHeight;
    settings.internalformat = GL_RGBA;
    settings.textureTarget = GL_TEXTURE_2D;
    settings.minFilter = GL_LINEAR;
    settings.maxFilter = GL_LINEAR;
    settings.wrapModeHorizontal = GL_CLAMP_TO_EDGE;
    settings.wrapModeVertical = GL_CLAMP_TO_EDGE;
    settings.numSamples = 0;
    
    outputFbo.allocate(settings);
}

void VideoMixer::resizeLayerArrays(int newSize) {
    int currentSize = layerOpacities.size();
    
    if (newSize > currentSize) {
        // Add new layers
        for (int i = currentSize; i < newSize; i++) {
            layerOpacities.push_back(ofParameter<float>("opacity_" + ofToString(i), 1.0, 0.0, 1.0));
            layerBlendModes.push_back(ofParameter<int>("blend_" + ofToString(i), 0, 0, (int)BlendMode::COUNT - 1));
            layerActive.push_back(ofParameter<bool>("active_" + ofToString(i), true));
            
            parameters.add(layerOpacities[i]);
            parameters.add(layerBlendModes[i]);
            parameters.add(layerActive[i]);
        }
    }
    // Note: We don't shrink arrays to preserve parameter references
}

int VideoMixer::addLayer() {
    if (numActiveLayers >= maxSupportedLayers) {
        ofLogWarning("VideoMixer") << "Cannot add layer: max " << maxSupportedLayers << " reached";
        return -1;
    }
    
    int newLayerIndex = numActiveLayers;
    
    // Ensure arrays are large enough
    if (newLayerIndex >= (int)layerOpacities.size()) {
        resizeLayerArrays(newLayerIndex + 4);  // Grow with some headroom
    }
    
    // Increment layer count
    numActiveLayers = newLayerIndex + 1;
    
    // Initialize defaults
    layerOpacities[newLayerIndex] = 1.0f;
    
    // Default blend modes: L1=ALPHA, then ADD/MULT alternating
    if (newLayerIndex == 0) {
        layerBlendModes[newLayerIndex] = (int)BlendMode::ALPHA;
    } else if (newLayerIndex % 2 == 1) {
        layerBlendModes[newLayerIndex] = (int)BlendMode::ADD;  // L2, L4, L6...
    } else {
        layerBlendModes[newLayerIndex] = (int)BlendMode::MULTIPLY;  // L3, L5, L7...
    }
    
    layerActive[newLayerIndex] = true;
    
    ofLogVerbose("VideoMixer") << "Added layer " << newLayerIndex << " (total: " << numActiveLayers << ")";
    return newLayerIndex;
}

void VideoMixer::removeLayer(int layerIndex) {
    if (layerIndex < 0 || layerIndex >= numActiveLayers) {
        return;
    }
    
    // Shift parameter arrays down (graph connections shifted by Graph::removeInput)
    for (int i = layerIndex; i < numActiveLayers - 1; i++) {
        layerOpacities[i] = layerOpacities[i + 1];
        layerBlendModes[i] = layerBlendModes[i + 1];
        layerActive[i] = layerActive[i + 1];
    }
    
    // Decrement count
    numActiveLayers--;
    
    ofLogVerbose("VideoMixer") << "Removed layer " << layerIndex << " (total: " << numActiveLayers << ")";
}

void VideoMixer::setLayerCount(int count) {
    int newCount = ofClamp(count, 1, maxSupportedLayers);
    
    // Ensure arrays are large enough
    if (newCount > layerOpacities.size()) {
        resizeLayerArrays(newCount + 4);
    }
    
    numActiveLayers = newCount;
    ofLogVerbose("VideoMixer") << "Set layer count to " << newCount;
}

int VideoMixer::getConnectedLayerCount() const {
    if (!graph) return 0;
    auto inputs = graph->getInputConnections(nodeIndex);
    int connected = 0;
    for (const auto& conn : inputs) {
        if (conn.toInput < numActiveLayers) connected++;
    }
    return connected;
}

Node* VideoMixer::getLayerSource(int layerIndex) const {
    if (!graph || layerIndex < 0 || layerIndex >= numActiveLayers) return nullptr;
    auto inputs = graph->getInputConnections(nodeIndex);
    for (const auto& conn : inputs) {
        if (conn.toInput == layerIndex) {
            return graph->getNode(conn.fromNode);
        }
    }
    return nullptr;
}

std::string VideoMixer::getLayerSourceName(int layerIndex) const {
    Node* source = getLayerSource(layerIndex);
    if (!source) {
        return "--";
    }
    return source->getDisplayName();
}

void VideoMixer::setLayerOpacity(int layer, float opacity) {
    if (layer >= 0 && layer < layerOpacities.size()) {
        layerOpacities[layer] = ofClamp(opacity, 0.0f, 1.0f);
    }
}

void VideoMixer::setLayerBlendMode(int layer, BlendMode mode) {
    if (layer >= 0 && layer < layerBlendModes.size()) {
        layerBlendModes[layer] = (int)mode;
    }
}

void VideoMixer::setLayerActive(int layer, bool active) {
    if (layer >= 0 && layer < layerActive.size()) {
        layerActive[layer] = active;
    }
}

float VideoMixer::getLayerOpacity(int layerIndex) const {
    if (layerIndex >= 0 && layerIndex < layerOpacities.size()) {
        return layerOpacities[layerIndex];
    }
    return 0.0f;
}

int VideoMixer::getLayerBlendMode(int layerIndex) const {
    if (layerIndex >= 0 && layerIndex < layerBlendModes.size()) {
        return layerBlendModes[layerIndex];
    }
    return 0;
}

bool VideoMixer::isLayerActive(int layerIndex) const {
    if (layerIndex >= 0 && layerIndex < layerActive.size()) {
        return layerActive[layerIndex];
    }
    return false;
}

bool VideoMixer::isLayerConnected(int layerIndex) const {
    return getLayerSource(layerIndex) != nullptr;
}

void VideoMixer::update(float dt) {
    // Derive sources from graph connections (single source of truth)
    auto inputs = graph->getInputConnections(nodeIndex);
    
    // Find all valid textures from connected sources
    std::vector<ofTexture*> validTextures;
    std::vector<float> validOpacities;
    std::vector<int> validBlendModes;
    
    for (int i = 0; i < numActiveLayers; i++) {
        if (!layerActive[i]) continue;
        
        // Find the source node connected to this input
        Node* sourceNode = nullptr;
        for (const auto& conn : inputs) {
            if (conn.toInput == i) {
                sourceNode = graph->getNode(conn.fromNode);
                break;
            }
        }
        
        if (sourceNode) {
            ofTexture* tex = sourceNode->getVideoOutput();
            if (tex && tex->isAllocated()) {
                validTextures.push_back(tex);
                validOpacities.push_back(layerOpacities[i]);
                validBlendModes.push_back(layerBlendModes[i]);
            }
        }
    }
    
    // If no valid textures, just clear and return
    if (validTextures.empty()) {
        outputFbo.begin();
        ofClear(0, 0, 0, 255);
        outputFbo.end();
        dirty = false;
        return;
    }
    
    // Render with proper blend modes
    outputFbo.begin();
    ofClear(0, 0, 0, 255);
    
    // Draw each valid layer with its blend mode
    for (size_t i = 0; i < validTextures.size(); i++) {
        BlendMode mode = (BlendMode)validBlendModes[i];
        
        switch (mode) {
            case BlendMode::ADD:
                ofEnableBlendMode(OF_BLENDMODE_ADD);
                break;
            case BlendMode::MULTIPLY:
                ofEnableBlendMode(OF_BLENDMODE_MULTIPLY);
                break;
            case BlendMode::ALPHA:
            default:
                ofEnableBlendMode(OF_BLENDMODE_ALPHA);
                break;
        }
        
        ofSetColor(255, validOpacities[i] * 255);
        validTextures[i]->draw(0, 0, fboWidth, fboHeight);
    }
    
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    ofSetColor(255);
    outputFbo.end();
    
    dirty = false;
}

ofTexture* VideoMixer::getVideoOutput() {
    return &outputFbo.getTexture();
}

void VideoMixer::deserializeComplete() {
    // Only resize if needed (don't reset already-loaded values)
    if ((int)layerOpacities.size() < numActiveLayers.get()) {
        resizeLayerArrays(numActiveLayers.get());
    }
    
    // Ensure FBO is allocated
    if (!outputFbo.isAllocated()) {
        allocateFbo();
    }
    
    // Mark dirty to force shader rebuild
    dirty = true;
}

ofJson VideoMixer::serialize() const {
    ofJson j;
    // Serialize ofParameters
    ofSerialize(j, parameters);
    // Serialize layer runtime values (not in ofParameters)
    j["layerOpacities"] = ofJson::array();
    j["layerBlendModes"] = ofJson::array();
    j["layerActive"] = ofJson::array();
    for (int i = 0; i < numActiveLayers; i++) {
        j["layerOpacities"].push_back(layerOpacities[i].get());
        j["layerBlendModes"].push_back(layerBlendModes[i].get());
        j["layerActive"].push_back(layerActive[i].get());
    }
    return j;
}

void VideoMixer::deserialize(const ofJson& json) {
    ofJson j = json;
    
    // Read layer arrays BEFORE unwrapping group (they're at root level)
    std::vector<float> savedOpacities;
    std::vector<int> savedBlendModes;
    std::vector<bool> savedActive;
    
    if (j.contains("layerOpacities")) {
        for (auto& v : j["layerOpacities"]) {
            savedOpacities.push_back(v.get<float>());
        }
    }
    if (j.contains("layerBlendModes")) {
        for (auto& v : j["layerBlendModes"]) {
            savedBlendModes.push_back(v.get<int>());
        }
    }
    if (j.contains("layerActive")) {
        for (auto& v : j["layerActive"]) {
            savedActive.push_back(v.get<bool>());
        }
    }
    
    // Handle nested "group" structure from ofSerialize
    if (j.contains("group")) {
        j = j["group"];
    }
    
    // Manually extract numLayers since ofDeserialize may have issues with the format
    if (j.contains("numLayers")) {
        int layers;
        if (j["numLayers"].is_string()) {
            layers = std::stoi(j["numLayers"].get<std::string>());
        } else {
            layers = j["numLayers"].get<int>();
        }
        numActiveLayers = layers;
        resizeLayerArrays(layers);
    }
    
    // Deserialize ofParameters
    ofDeserialize(j, parameters);
    
    // Apply saved layer values (from serialize() custom format)
    for (int i = 0; i < savedOpacities.size() && i < layerOpacities.size(); i++) {
        layerOpacities[i] = savedOpacities[i];
    }
    for (int i = 0; i < savedBlendModes.size() && i < layerBlendModes.size(); i++) {
        layerBlendModes[i] = savedBlendModes[i];
    }
    for (int i = 0; i < savedActive.size() && i < layerActive.size(); i++) {
        layerActive[i] = savedActive[i];
    }
}
