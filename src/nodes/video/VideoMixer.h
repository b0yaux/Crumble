#pragma once
#include "../../core/Node.h"

// Blend modes for video compositing
enum class BlendMode {
    ALPHA = 0,
    ADD,
    MULTIPLY,
    COUNT
};

// Multi-layer video mixer using single-pass shader with dynamic layer support
// Supports up to 64-128 layers depending on GPU (M1 Pro: 128 texture units)
class VideoMixer : public Node {
public:
    VideoMixer();
    ~VideoMixer();
    
    void setup(int width, int height);
    
    void update(float dt) override;
    ofTexture* getVideoOutput() override;
    
    // Dynamic layer management
    int addLayer(Node* sourceNode);           // Returns layer index, -1 if full
    void removeLayer(int layerIndex);
    void setLayerCount(int count);            // Set number of active layers
    int getLayerCount() const { return numActiveLayers; }
    int getMaxSupportedLayers() const { return maxSupportedLayers; }
    int getConnectedLayerCount() const;       // How many layers have connections
    
    // Layer configuration
    void setLayerOpacity(int layer, float opacity);
    void setLayerBlendMode(int layer, BlendMode mode);
    void setLayerActive(int layer, bool active);
    
    // Get info about a layer
    Node* getLayerSource(int layerIndex) const;
    void setLayerSource(int layerIndex, Node* sourceNode);
    float getLayerOpacity(int layerIndex) const;
    int getLayerBlendMode(int layerIndex) const;
    bool isLayerActive(int layerIndex) const;
    bool isLayerConnected(int layerIndex) const;
    
    // Parameter for GUI
    ofParameter<int> numActiveLayers;
    
    // Precompile shaders for common layer counts (call after setup)
    void precompileShaders(const std::vector<int>& layerCounts);
    
private:
    void allocateFbo();
    ofShader& getShader(int layerCount);      // Get/cached shader for N layers
    void buildShader(int layerCount);         // Build shader for N layers
    int nextPowerOf2(int n) const;            // Helper for cache sizing
    void detectGpuLimits();                   // Query GL_MAX_TEXTURE_IMAGE_UNITS
    void resizeLayerArrays(int newSize);      // Resize parameter arrays
    
    int fboWidth = 1920;
    int fboHeight = 1080;
    int maxSupportedLayers = 64;              // Detected from GPU, default 64
    
    ofFbo outputFbo;
    ofVboMesh fullscreenQuad;
    
    // Shader cache - key is layer count (rounded to power of 2)
    std::unordered_map<int, ofShader> shaderCache;
    
    // Per-layer parameters (dynamically sized)
    std::vector<ofParameter<float>> layerOpacities;
    std::vector<ofParameter<int>> layerBlendModes;
    std::vector<ofParameter<bool>> layerActive;
    std::vector<Node*> layerSources;          // Track connected source nodes
    
    // Default black texture for unconnected/inactive layers
    ofTexture defaultTexture;
    bool defaultTextureAllocated = false;
};
