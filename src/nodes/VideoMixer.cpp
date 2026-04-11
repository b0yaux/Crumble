#include "ofMain.h"
#include "VideoMixer.h"
#include "../core/Graph.h"
#include "../core/NodeProcessor.h"

namespace crumble {

class VideoMixerProcessor : public VideoProcessor {
public:
    ControlSlot* numLayersSlot = nullptr;
    ControlSlot* masterOpacitySlot = nullptr;
    std::array<ControlSlot*, MAX_INPUTS> opacitySlots = {nullptr};
    std::array<ControlSlot*, MAX_INPUTS> blendSlots = {nullptr};
    std::array<ControlSlot*, MAX_INPUTS> activeSlots = {nullptr};
    ofShader compositeShader;
    ofMesh quadMesh;
    bool shaderLoaded = false;

    VideoMixerProcessor() {
        numLayersSlot = getControlPtr(crumble::hashString("numLayers"));
        masterOpacitySlot = getControlPtr(crumble::hashString("opacity"));
    }

    void buildQuad(float w, float h) {
        quadMesh.clear();
        quadMesh.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
        quadMesh.addVertex(glm::vec3(0, 0, 0));
        quadMesh.addVertex(glm::vec3(w, 0, 0));
        quadMesh.addVertex(glm::vec3(w, h, 0));
        quadMesh.addVertex(glm::vec3(0, h, 0));
        quadMesh.addTexCoord(glm::vec2(0, 0));
        quadMesh.addTexCoord(glm::vec2(1, 0));
        quadMesh.addTexCoord(glm::vec2(1, 1));
        quadMesh.addTexCoord(glm::vec2(0, 1));
    }

    void addInput(VideoProcessor* p, int toInput, int fromOutput) override {
        VideoProcessor::addInput(p, toInput, fromOutput);
        std::string prefix = std::to_string(toInput);
        opacitySlots[toInput] = getControlPtr(crumble::hashString(("opacity_" + prefix).c_str()));
        blendSlots[toInput] = getControlPtr(crumble::hashString(("blend_" + prefix).c_str()));
        activeSlots[toInput] = getControlPtr(crumble::hashString(("active_" + prefix).c_str()));
    }

    ofFbo* resultFbo = nullptr;

    ofTexture* getOutput(int index = 0) override {
        if (resultFbo && resultFbo->isAllocated()) {
            return &resultFbo->getTexture();
        }
        return nullptr;
    }

    void processVideo(double cycle, double cycleStep) override {
        currentCycle = cycle;

        if (!shaderLoaded) {
            compositeShader.load(
                ofToDataPath("shaders/composite.vert"),
                ofToDataPath("shaders/composite.frag")
            );
            shaderLoaded = compositeShader.isLoaded();
            if (!shaderLoaded) {
                ofLogError("VideoMixer") << "Failed to load composite shader";
                return;
            }
            ofLogNotice("VideoMixer") << "Composite shader loaded successfully";
        }

        int numActiveLayers = std::min((int)evalSlot(numLayersSlot, cycle), MAX_INPUTS);
        float masterOpacity = evalSlot(masterOpacitySlot, cycle);

        struct LayerData {
            ofTexture* tex;
            float opacity;
            int blendMode;
        };

        std::vector<LayerData> activeLayers;
        activeLayers.reserve(numActiveLayers);

        for (int i = 0; i < numActiveLayers; i++) {
            if (i >= MAX_INPUTS || !inputs[i].processor) continue;

            float opacity = evalSlot(opacitySlots[i], cycle);
            float blendModeVal = evalSlot(blendSlots[i], cycle);
            float active = evalSlot(activeSlots[i], cycle);

            if (active < 0.5f) continue;

            ofTexture* tex = inputs[i].processor->getOutput(inputs[i].fromOutput);
            if (!tex || !tex->isAllocated()) continue;

            float sourceOpacity = inputs[i].processor->getParam(crumble::hashString("opacity"));
            float sourceActive = inputs[i].processor->getParam(crumble::hashString("active"));
            if (sourceActive < 0.5f) continue;

            int finalBlend = (int)inputs[i].processor->getParam(crumble::hashString("blend"));
            int mixerBlend = (int)blendModeVal;
            if (mixerBlend >= 0) finalBlend = mixerBlend;
            finalBlend = std::max(0, finalBlend);

            activeLayers.push_back({tex, opacity * masterOpacity * sourceOpacity, finalBlend});
        }

        int layerCount = (int)activeLayers.size();

        int fbW = 1920, fbH = 1080;
        if (layerCount > 0 && activeLayers[0].tex) {
            fbW = activeLayers[0].tex->getWidth();
            fbH = activeLayers[0].tex->getHeight();
        }

        if (!fboA.isAllocated() || fboA.getWidth() != fbW) {
            ofFbo::Settings fboSettings;
            fboSettings.width = fbW;
            fboSettings.height = fbH;
            fboSettings.internalformat = GL_RGBA;
            fboSettings.textureTarget = GL_TEXTURE_2D;
            fboA.allocate(fboSettings);
            fboB.allocate(fboSettings);
            buildQuad(fboSettings.width, fboSettings.height);
        }

        if (layerCount == 0) {
            fboA.begin();
            ofClear(0, 0, 0, 255);
            fboA.end();
            resultFbo = &fboA;
            return;
        }

        static constexpr int CHUNK = 15;
        int numChunks = (layerCount + CHUNK - 1) / CHUNK;

        static int logCounter = 0;
        if (++logCounter % 120 == 0 && layerCount > 0) {
            for (int i = 0; i < layerCount; i++) {
                ofLogVerbose("VideoMixer") << "layer[" << i << "] opacity=" << activeLayers[i].opacity
                    << " blend=" << activeLayers[i].blendMode;
            }
            ofLogVerbose("VideoMixer") << "  layerCount=" << layerCount << " numChunks=" << numChunks;
        }

        ofFbo* writeFbo = &fboA;
        ofFbo* readFbo = &fboB;

        fboA.begin(); ofClear(0, 0, 0, 255); fboA.end();
        fboB.begin(); ofClear(0, 0, 0, 255); fboB.end();

        for (int chunk = 0; chunk < numChunks; chunk++) {
            int chunkStart = chunk * CHUNK;
            int chunkEnd = std::min(chunkStart + CHUNK, layerCount);
            int chunkSize = chunkEnd - chunkStart;

            float opacities[CHUNK];
            int blendModes[CHUNK];

            writeFbo->begin();

            compositeShader.begin();
            compositeShader.setUniform1i("uNumLayers", chunkSize);

            for (int i = 0; i < chunkSize; i++) {
                auto& layer = activeLayers[chunkStart + i];
                ofTextureData& td = layer.tex->getTextureData();
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(td.textureTarget, td.textureID);
                opacities[i] = layer.opacity;
                blendModes[i] = layer.blendMode;
            }

            int samplerUnits[CHUNK];
            for (int i = 0; i < chunkSize; i++) samplerUnits[i] = i;
            compositeShader.setUniform1iv("uTextures", samplerUnits, chunkSize);

            if (chunk > 0) {
                ofTextureData& accTd = readFbo->getTexture().getTextureData();
                glActiveTexture(GL_TEXTURE0 + 15);
                glBindTexture(accTd.textureTarget, accTd.textureID);
                compositeShader.setUniform1i("uAccumTex", 15);
            }

            compositeShader.setUniform1fv("uOpacities", opacities, chunkSize);
            compositeShader.setUniform1iv("uBlendModes", blendModes, chunkSize);
            compositeShader.setUniform1i("uHasAccum", chunk > 0 ? 1 : 0);

            quadMesh.draw();

            compositeShader.end();
            writeFbo->end();

            std::swap(writeFbo, readFbo);
        }

        resultFbo = readFbo;
    }

private:
    ofFbo fboA;
    ofFbo fboB;
};

} // namespace crumble

VideoMixer::VideoMixer() {
    type = "videomix";
    
    // Add parameters
    parameters->add(numActiveLayers.set("numLayers", 2, 1, 128));
    
    // Add listener to react immediately to layer count changes
    numActiveLayers.addListener(this, &VideoMixer::onNumLayersChanged);
    
    // Initialize with default 2 layers
    resizeLayerArrays(2);
}

VideoMixer::~VideoMixer() {
    numActiveLayers.removeListener(this, &VideoMixer::onNumLayersChanged);
}

void VideoMixer::resizeLayerArrays(int newSize) {
    int currentSize = (int)layerOpacities.size();
    
    if (newSize > currentSize) {
        // Add new layers
        for (int i = currentSize; i < newSize; i++) {
            layerOpacities.push_back(ofParameter<float>("opacity_" + ofToString(i), 1.0, 0.0, 1.0));
            layerBlendModes.push_back(ofParameter<int>("blend_" + ofToString(i), -1, -1, (int)BlendMode::COUNT - 1));
            layerActive.push_back(ofParameter<bool>("active_" + ofToString(i), true));
            
            parameters->add(layerOpacities[i]);
            parameters->add(layerBlendModes[i]);
            parameters->add(layerActive[i]);
            
            // Sync new layer params to VideoMixerProcessor
            if (videoProcessor) {
                if (auto* slot = videoProcessor->getControlPtr(crumble::hashString(("opacity_" + std::to_string(i)).c_str()))) slot->value.store(1.0f, std::memory_order_relaxed);
                if (auto* slot = videoProcessor->getControlPtr(crumble::hashString(("blend_" + std::to_string(i)).c_str()))) slot->value.store(-1.0f, std::memory_order_relaxed);
                if (auto* slot = videoProcessor->getControlPtr(crumble::hashString(("active_" + std::to_string(i)).c_str()))) slot->value.store(1.0f, std::memory_order_relaxed);
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

void VideoMixer::setLayerCount(int count) {
    numActiveLayers = count; 
    if (videoProcessor) {
        if (auto* slot = videoProcessor->getControlPtr(crumble::hashString("numLayers"))) slot->value.store((float)count, std::memory_order_relaxed);
    }
}

int VideoMixer::addLayer() {
    if (numActiveLayers >= maxSupportedLayers) {
        return -1;
    }
    int newLayerIndex = numActiveLayers;
    numActiveLayers = newLayerIndex + 1;
    layerOpacities[newLayerIndex] = 1.0f;
    if (newLayerIndex == 0) {
        layerBlendModes[newLayerIndex] = (int)BlendMode::ALPHA;
    } else if (newLayerIndex % 2 == 1) {
        layerBlendModes[newLayerIndex] = (int)BlendMode::ADD;
    } else {
        layerBlendModes[newLayerIndex] = (int)BlendMode::MULTIPLY;
    }
    layerActive[newLayerIndex] = true;
    return newLayerIndex;
}

void VideoMixer::onInputConnected(int toInput) {
    if (toInput >= numActiveLayers) {
        setLayerCount(toInput + 1);
    }
}

int VideoMixer::getConnectedLayerCount() const {
    int connected = 0;
    for (const auto& [slot, node] : inputNodes) {
        if (slot < numActiveLayers) connected++;
    }
    return connected;
}

Node* VideoMixer::getLayerSource(int layerIndex) const {
    if (layerIndex < 0 || layerIndex >= numActiveLayers) return nullptr;
    return getInputNode(layerIndex);
}

std::string VideoMixer::getLayerSourceName(int layerIndex) const {
    Node* source = getLayerSource(layerIndex);
    if (!source) return "--";
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
    return new crumble::VideoMixerProcessor();
}

ofTexture* VideoMixer::processVideo(int index) {
    if (index != 0 || !videoProcessor) return nullptr;
    return videoProcessor->getOutput(0);
}

ofJson VideoMixer::serialize() const {
    ofJson j;
    ofSerialize(j, *parameters);
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
    if (j.contains("group")) j = j["group"];
    else if (j.contains("params")) j = j["params"];
    
    if (j.contains("numLayers")) {
        numActiveLayers = getSafeJson<int>(j, "numLayers", numActiveLayers.get());
        setLayerCount(numActiveLayers);
    }
    if (j.contains("opacity")) {
        opacity->set(getSafeJson<float>(j, "opacity", opacity->get()));
    }
    ofDeserialize(j, *parameters);
    
    for (int i = 0; i < numActiveLayers; i++) {
        string opKey = "opacity_" + ofToString(i);
        if (j.contains(opKey)) layerOpacities[i] = getSafeJson<float>(j, opKey, layerOpacities[i].get());
        string blKey = "blend_" + ofToString(i);
        if (j.contains(blKey)) layerBlendModes[i] = getSafeJson<int>(j, blKey, layerBlendModes[i].get());
        string acKey = "active_" + ofToString(i);
        if (j.contains(acKey)) layerActive[i] = getSafeJson<bool>(j, acKey, layerActive[i].get());
    }
}
