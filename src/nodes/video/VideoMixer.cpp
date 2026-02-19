#include "VideoMixer.h"
#include "../../core/Graph.h"

VideoMixer::VideoMixer() {
    type = "VideoMixer";
    
    // Add numActiveLayers parameter
    parameters.add(numActiveLayers.set("numLayers", 2, 1, 128));
    
    // Initialize with default 2 layers
    resizeLayerArrays(2);
    
    // Create fullscreen quad
    fullscreenQuad.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
    fullscreenQuad.addVertex(glm::vec3(-1, -1, 0));
    fullscreenQuad.addVertex(glm::vec3(1, -1, 0));
    fullscreenQuad.addVertex(glm::vec3(1, 1, 0));
    fullscreenQuad.addVertex(glm::vec3(-1, 1, 0));
    fullscreenQuad.addTexCoord(glm::vec2(0, 0));
    fullscreenQuad.addTexCoord(glm::vec2(1, 0));
    fullscreenQuad.addTexCoord(glm::vec2(1, 1));
    fullscreenQuad.addTexCoord(glm::vec2(0, 1));
}

VideoMixer::~VideoMixer() {
    // Shaders automatically cleaned up by unordered_map destructor
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
    
    // Precompile common layer counts
    precompileShaders({1, 2, 4, 8, 16, 32, 64});
    
    // Ensure we have shader for current layer count
    getShader(numActiveLayers);
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
    
    // Create 1x1 black default texture
    if (!defaultTextureAllocated) {
        ofPixels pixels;
        pixels.allocate(1, 1, OF_PIXELS_RGBA);
        pixels.setColor(0, 0, ofColor(0, 0, 0, 255));
        defaultTexture.allocate(pixels);
        defaultTextureAllocated = true;
    }
}

int VideoMixer::nextPowerOf2(int n) const {
    if (n <= 1) return 1;
    if ((n & (n - 1)) == 0) return n;  // Already power of 2
    
    int power = 1;
    while (power < n) {
        power <<= 1;
    }
    return power;
}

void VideoMixer::buildShader(int layerCount) {
    // Build shader string for specified number of layers
    string frag;
    
    // Uniforms for all layers
    for (int i = 0; i < layerCount; i++) {
        frag += "uniform sampler2D tex" + ofToString(i) + ";\n";
    }
    
    // Uniform arrays for layer properties
    frag += "uniform float uOpacity[" + ofToString(layerCount) + "];\n";
    frag += "uniform int uBlendMode[" + ofToString(layerCount) + "];\n";
    frag += "uniform int uActive[" + ofToString(layerCount) + "];\n";
    
    // Simple blend function (3 modes)
    frag += "vec4 blend(vec4 base, vec4 layer, int mode) {\n";
    frag += "    if (mode == 0) return mix(base, layer, layer.a);\n";  // ALPHA
    frag += "    else if (mode == 1) return base + layer;\n";          // ADD
    frag += "    else return base * layer;\n";                         // MULTIPLY
    frag += "}\n\n";
    
    frag += "void main() {\n";
    frag += "    vec4 result = vec4(0.0, 0.0, 0.0, 1.0);\n";
    frag += "    vec2 texCoord = gl_TexCoord[0].st;\n\n";
    
    // Sample and blend each layer
    for (int i = 0; i < layerCount; i++) {
        frag += "    if (uActive[" + ofToString(i) + "] == 1) {\n";
        frag += "        vec4 layer = texture2D(tex" + ofToString(i) + ", texCoord) * uOpacity[" + ofToString(i) + "];\n";
        frag += "        result = blend(result, layer, uBlendMode[" + ofToString(i) + "]);\n";
        frag += "    }\n";
    }
    
    frag += "    gl_FragColor = result;\n";
    frag += "}\n";
    
    // Create and compile shader
    ofShader shader;
    shader.setupShaderFromSource(GL_FRAGMENT_SHADER, frag);
    shader.linkProgram();
    
    // Store in cache
    shaderCache[layerCount] = shader;
    
    ofLogVerbose("VideoMixer") << "Built shader for " << layerCount << " layers";
}

ofShader& VideoMixer::getShader(int layerCount) {
    // Round up to nearest power of 2 for cache efficiency
    int cacheKey = nextPowerOf2(layerCount);
    
    // Check if shader exists in cache
    auto it = shaderCache.find(cacheKey);
    if (it != shaderCache.end()) {
        return it->second;
    }
    
    // Build shader if not cached
    buildShader(cacheKey);
    return shaderCache[cacheKey];
}

void VideoMixer::precompileShaders(const std::vector<int>& layerCounts) {
    for (int count : layerCounts) {
        if (count <= maxSupportedLayers) {
            getShader(count);
        }
    }
    ofLogNotice("VideoMixer") << "Precompiled " << shaderCache.size() << " shaders";
}

void VideoMixer::resizeLayerArrays(int newSize) {
    int currentSize = layerOpacities.size();
    
    if (newSize > currentSize) {
        // Add new layers
        for (int i = currentSize; i < newSize; i++) {
            layerOpacities.push_back(ofParameter<float>("opacity_" + ofToString(i), 1.0, 0.0, 1.0));
            layerBlendModes.push_back(ofParameter<int>("blend_" + ofToString(i), 0, 0, (int)BlendMode::COUNT - 1));
            layerActive.push_back(ofParameter<bool>("active_" + ofToString(i), true));
            layerSources.push_back(nullptr);
            
            parameters.add(layerOpacities[i]);
            parameters.add(layerBlendModes[i]);
            parameters.add(layerActive[i]);
        }
    }
    // Note: We don't shrink arrays to preserve parameter references
}

int VideoMixer::addLayer(Node* sourceNode) {
    if (numActiveLayers >= maxSupportedLayers) {
        ofLogWarning("VideoMixer") << "Cannot add layer: max " << maxSupportedLayers << " reached";
        return -1;
    }
    
    int newLayerIndex = numActiveLayers;
    
    // Ensure arrays are large enough
    if (newLayerIndex >= layerOpacities.size()) {
        resizeLayerArrays(newLayerIndex + 4);  // Grow with some headroom
    }
    
    // Increment layer count
    numActiveLayers = newLayerIndex + 1;
    
    // Store source reference
    layerSources[newLayerIndex] = sourceNode;
    
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
    
    ofLogNotice("VideoMixer") << "Added layer " << newLayerIndex << " (total: " << numActiveLayers << ")";
    return newLayerIndex;
}

void VideoMixer::removeLayer(int layerIndex) {
    if (layerIndex < 0 || layerIndex >= numActiveLayers) {
        return;
    }
    
    // Shift all layers above down
    for (int i = layerIndex; i < numActiveLayers - 1; i++) {
        layerSources[i] = layerSources[i + 1];
        layerOpacities[i] = layerOpacities[i + 1];
        layerBlendModes[i] = layerBlendModes[i + 1];
        layerActive[i] = layerActive[i + 1];
    }
    
    // Clear the last layer
    layerSources[numActiveLayers - 1] = nullptr;
    
    // Decrement count
    numActiveLayers--;
    
    ofLogNotice("VideoMixer") << "Removed layer " << layerIndex << " (total: " << numActiveLayers << ")";
}

void VideoMixer::setLayerCount(int count) {
    int newCount = ofClamp(count, 1, maxSupportedLayers);
    
    // Ensure arrays are large enough
    if (newCount > layerOpacities.size()) {
        resizeLayerArrays(newCount + 4);
    }
    
    numActiveLayers = newCount;
    ofLogNotice("VideoMixer") << "Set layer count to " << newCount;
}

int VideoMixer::getConnectedLayerCount() const {
    int connected = 0;
    for (int i = 0; i < numActiveLayers; i++) {
        if (layerSources[i] != nullptr) {
            connected++;
        }
    }
    return connected;
}

Node* VideoMixer::getLayerSource(int layerIndex) const {
    if (layerIndex >= 0 && layerIndex < layerSources.size()) {
        return layerSources[layerIndex];
    }
    return nullptr;
}

void VideoMixer::setLayerSource(int layerIndex, Node* sourceNode) {
    if (layerIndex >= 0 && layerIndex < layerSources.size()) {
        layerSources[layerIndex] = sourceNode;
    }
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
    if (layerIndex >= 0 && layerIndex < layerSources.size()) {
        return layerSources[layerIndex] != nullptr;
    }
    return false;
}

void VideoMixer::update(float dt) {
    // Pull inputs from connected nodes
    auto inputs = graph->getInputConnections(nodeIndex);
    for (const auto& conn : inputs) {
        if (conn.toInput < numActiveLayers) {
            Node* sourceNode = graph->getNode(conn.fromNode);
            if (sourceNode) {
                layerSources[conn.toInput] = sourceNode;
                sourceNode->update(dt);
            }
        }
    }
    
    // Find all valid textures from connected sources
    std::vector<ofTexture*> validTextures;
    std::vector<float> validOpacities;
    std::vector<int> validBlendModes;
    
    for (int i = 0; i < numActiveLayers; i++) {
        if (layerSources[i] && layerActive[i]) {
            ofTexture* tex = layerSources[i]->getVideoOutput();
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
    
    ofEnableBlendMode(OF_BLENDMODE_DISABLED);
    outputFbo.end();
    
    dirty = false;
}

ofTexture* VideoMixer::getVideoOutput() {
    return &outputFbo.getTexture();
}
