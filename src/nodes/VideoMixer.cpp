#include "ofMain.h"
#include "VideoMixer.h"
#include "../core/Graph.h"
#include "../core/NodeProcessor.h"

namespace crumble {

class VideoMixerProcessor : public VideoProcessor {
public:
    void processVideo(double cycle, double cycleStep) override {
        if (!writeTex) return;
        
        int numActiveLayers = std::min((int)getParam("numLayers"), 16);
        float masterOpacity = getParam("masterOpacity");
        
        // Use ping-pong FBOs to avoid clearing the texture being read by ScreenOutput
        ofFbo& currentFbo = (writeTex == &tex_A) ? fboA : fboB;
        
        if (!currentFbo.isAllocated() || currentFbo.getWidth() != writeTex->getWidth()) {
            currentFbo.allocate(writeTex->getWidth(), writeTex->getHeight(), GL_RGBA);
        }

        currentFbo.begin();
        ofClear(0, 0, 0, 0); 
        ofSetColor(20);
        ofDrawRectangle(0, 0, currentFbo.getWidth(), currentFbo.getHeight()); // Background
        
        for (int i = 0; i < numActiveLayers; i++) {
            if (inputs[i].processor) {
                float opacity = evalPattern("opacity_" + std::to_string(i), cycle);
                float blendModeVal = evalPattern("blend_" + std::to_string(i), cycle);
                float active = evalPattern("active_" + std::to_string(i), cycle);
                
                if (active > 0.5f) {
                    ofTexture* tex = inputs[i].processor->getOutput(inputs[i].fromOutput);
                    if (tex && tex->isAllocated()) {
                        BlendMode mode = (BlendMode)(int)blendModeVal;
                        switch (mode) {
                            case BlendMode::ADD: ofEnableBlendMode(OF_BLENDMODE_ADD); break;
                            case BlendMode::MULTIPLY: ofEnableBlendMode(OF_BLENDMODE_MULTIPLY); break;
                            case BlendMode::ALPHA:
                            default: ofEnableBlendMode(OF_BLENDMODE_ALPHA); break;
                        }
                        
                        // Get source metadata dynamically from the source processor each frame
                        float sourceOpacity = inputs[i].processor->getParam("opacity");
                        float sourceActive = inputs[i].processor->getParam("active");
                        
                        if (sourceActive > 0.5f) {
                            ofSetColor(255, opacity * masterOpacity * sourceOpacity * 255);
                            tex->draw(0, 0, currentFbo.getWidth(), currentFbo.getHeight());
                        }
                    }
                }
            }
        }
        
        ofDisableBlendMode();
        currentFbo.end();
        
        // Copy FBO texture reference to our shadow texture
        *writeTex = currentFbo.getTexture();
        
        swapFbo(); 
    }

private:
    ofFbo fboA;
    ofFbo fboB;
};

} // namespace crumble

VideoMixer::VideoMixer() {
    type = "VideoMixer";
    
    // Add parameters
    parameters->add(numActiveLayers.set("numLayers", 2, 1, 128));
    parameters->add(masterOpacity.set("masterOpacity", 1.0, 0.0, 1.0));
    
    // Add listener to react immediately to layer count changes
    numActiveLayers.addListener(this, &VideoMixer::onNumLayersChanged);
    
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
}



void VideoMixer::resizeLayerArrays(int newSize) {
    int currentSize = (int)layerOpacities.size();
    
    if (newSize > currentSize) {
        // Add new layers
        for (int i = currentSize; i < newSize; i++) {
            layerOpacities.push_back(ofParameter<float>("opacity_" + ofToString(i), 1.0, 0.0, 1.0));
            layerBlendModes.push_back(ofParameter<int>("blend_" + ofToString(i), 0, 0, (int)BlendMode::COUNT - 1));
            layerActive.push_back(ofParameter<bool>("active_" + ofToString(i), true));
            
            parameters->add(layerOpacities[i]);
            parameters->add(layerBlendModes[i]);
            parameters->add(layerActive[i]);
            
            // Sync new layer params to VideoMixerProcessor
            if (videoProcessor) {
                videoProcessor->valuesMap["opacity_" + std::to_string(i)].store(1.0f);
                videoProcessor->valuesMap["blend_" + std::to_string(i)].store(0.0f);
                videoProcessor->valuesMap["active_" + std::to_string(i)].store(1.0f);
            }
        }
    } else if (newSize < currentSize) {
        // Remove excess layers
        for (int i = currentSize - 1; i >= newSize; i--) {
            parameters->remove(layerOpacities[i]);
            parameters->remove(layerBlendModes[i]);
            parameters->remove(layerActive[i]);
            
            layerOpacities.pop_back();
            layerBlendModes.pop_back();
            layerActive.pop_back();
        }
    }
}

void VideoMixer::onNumLayersChanged(int& count) {
    int newCount = ofClamp(count, 1, maxSupportedLayers);
    
    // Sync arrays to the new count (handles both growth and shrinkage)
    resizeLayerArrays(newCount);
    
    // Update the parameter itself to the clamped value without triggering listener again
    numActiveLayers.setWithoutEventNotifications(newCount);
    
    ofLogVerbose("VideoMixer") << "Set layer count to " << newCount;
}

void VideoMixer::removeLayer(int layerIndex) {
    if (layerIndex < 0 || layerIndex >= numActiveLayers) {
        return;
    }
    
    // 1. Disconnect the graph connection for this layer
    if (graph) {
        graph->disconnect(nodeId, layerIndex);
        // Shift higher-numbered connection indices down to close the gap
        graph->compactInputIndices(nodeId, layerIndex);
    }
    
    // 2. Shift per-layer parameter values down
    for (int i = layerIndex; i < numActiveLayers - 1; i++) {
        layerOpacities[i] = layerOpacities[i + 1];
        layerBlendModes[i] = layerBlendModes[i + 1];
        layerActive[i] = layerActive[i + 1];
    }
    
    // 3. Shrink layer count
    numActiveLayers--;
    
    ofLogVerbose("VideoMixer") << "Removed layer " << layerIndex << " (total: " << numActiveLayers << ")";
}

void VideoMixer::setLayerCount(int count) {
    numActiveLayers = count; // This will trigger onNumLayersChanged
    
    // Sync to VideoMixerProcessor
    if (videoProcessor) {
        videoProcessor->valuesMap["numLayers"].store((float)count);
    }
}

int VideoMixer::addLayer() {
    if (numActiveLayers >= maxSupportedLayers) {
        ofLogWarning("VideoMixer") << "Cannot add layer: max " << maxSupportedLayers << " reached";
        return -1;
    }
    
    int newLayerIndex = numActiveLayers;
    
    // Increment layer count (triggers onNumLayersChanged)
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

void VideoMixer::onInputConnected(int toInput) {
    if (toInput >= numActiveLayers) {
        setLayerCount(toInput + 1);
    }
}

int VideoMixer::getConnectedLayerCount() const {
    if (!graph) return 0;
    auto inputs = graph->getInputConnections(nodeId);
    int connected = 0;
    for (const auto& conn : inputs) {
        if (conn.toInput < numActiveLayers) connected++;
    }
    return connected;
}

Node* VideoMixer::getLayerSource(int layerIndex) const {
    if (!graph || layerIndex < 0 || layerIndex >= numActiveLayers) return nullptr;
    auto inputs = graph->getInputConnections(nodeId);
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
    if (layer >= 0 && layer < (int)layerOpacities.size()) {
        layerOpacities[layer] = ofClamp(opacity, 0.0f, 1.0f);
    }
}

void VideoMixer::setLayerBlendMode(int layer, BlendMode mode) {
    if (layer >= 0 && layer < (int)layerBlendModes.size()) {
        layerBlendModes[layer] = (int)mode;
    }
}

void VideoMixer::setLayerActive(int layer, bool active) {
    if (layer >= 0 && layer < (int)layerActive.size()) {
        layerActive[layer] = active;
    }
}

float VideoMixer::getLayerOpacity(int layerIndex) const {
    if (layerIndex >= 0 && layerIndex < (int)layerOpacities.size()) {
        return layerOpacities[layerIndex];
    }
    return 0.0f;
}

int VideoMixer::getLayerBlendMode(int layerIndex) const {
    if (layerIndex >= 0 && layerIndex < (int)layerBlendModes.size()) {
        return layerBlendModes[layerIndex];
    }
    return 0;
}

bool VideoMixer::isLayerActive(int layerIndex) const {
    if (layerIndex >= 0 && layerIndex < (int)layerActive.size()) {
        return layerActive[layerIndex];
    }
    return false;
}

bool VideoMixer::isLayerConnected(int layerIndex) const {
    return getLayerSource(layerIndex) != nullptr;
}

crumble::VideoProcessor* VideoMixer::createVideoProcessor() {
    auto proc = new crumble::VideoMixerProcessor();
    proc->allocateTextures(fboWidth, fboHeight);
    return proc;
}

ofTexture* VideoMixer::processVideo(int index) {
    if (index != 0 || !videoProcessor) return nullptr;
    return videoProcessor->getOutput(0);
}


ofJson VideoMixer::serialize() const {
    ofJson j;
    // Serialize ofParameters
    ofSerialize(j, *parameters);
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
    
    // 1. Handle "group" unwrapping if present
    if (j.contains("group")) {
        j = j["group"];
    } else if (j.contains("params")) {
        j = j["params"];
    }
    
    // 2. Extract numLayers FIRST to resize arrays
    if (j.contains("numLayers")) {
        numActiveLayers = getSafeJson<int>(j, "numLayers", numActiveLayers.get());
        setLayerCount(numActiveLayers);
    }
    
    if (j.contains("masterOpacity")) {
        masterOpacity = getSafeJson<float>(j, "masterOpacity", masterOpacity.get());
    }
    
    // 3. Deserialize ofParameters (covers numLayers, and any named opacity_n if they match)
    // We still call this for general parameters, but manual sync below handles "loose" types
    ofDeserialize(j, *parameters);
    
    // 4. Fallback for custom array format (legacy support)
    if (json.contains("layerOpacities")) {
        auto& arr = json["layerOpacities"];
        for (int i = 0; i < (int)arr.size() && i < (int)layerOpacities.size(); i++) {
            layerOpacities[i] = arr[i].get<float>();
        }
    }
    // ... (blend and active fallbacks omitted for brevity if they match JSON structure)
    
    // 5. Explicitly sync numbered params with "loose" type support to prevent Abort trap
    for (int i = 0; i < numActiveLayers; i++) {
        string opKey = "opacity_" + ofToString(i);
        if (j.contains(opKey)) layerOpacities[i] = getSafeJson<float>(j, opKey, layerOpacities[i].get());
        
        string blKey = "blend_" + ofToString(i);
        if (j.contains(blKey)) layerBlendModes[i] = getSafeJson<int>(j, blKey, layerBlendModes[i].get());
        
        string acKey = "active_" + ofToString(i);
        if (j.contains(acKey)) layerActive[i] = getSafeJson<bool>(j, acKey, layerActive[i].get());
    }
}
