#pragma once
#include "ofMain.h"
#include "../core/Node.h"

// Blend modes for video compositing
enum class BlendMode {
    ALPHA = 0,
    ADD,
    MULTIPLY,
    SCREEN,
    COUNT
};

// Multi-layer video mixer using single-pass shader with dynamic layer support
// Supports up to 64-128 layers depending on GPU (M1 Pro: 128 texture units)
class VideoMixer : public Node {
public:
    VideoMixer();
    ~VideoMixer();
    
    crumble::VideoProcessor* createVideoProcessor() override;
    ofTexture* processVideo(int index = 0) override;
    
    // React to graph connections
    void onInputConnected(int toInput) override;
    
    // Dynamic layer management

    int addLayer();                            // Returns layer index, -1 if full
    void setLayerCount(int count);
    int getLayerCount() const { return numActiveLayers; }
    int getMaxSupportedLayers() const { return maxSupportedLayers; }
    int getConnectedLayerCount() const;       // How many layers have connections
    
    // Layer configuration
    void setLayerOpacity(int layer, float opacity);
    void setLayerBlendMode(int layer, BlendMode mode);
    void setLayerActive(int layer, bool active);
    
    // Get info about a layer (queries graph connections, no stored pointers)
    Node* getLayerSource(int layerIndex) const;
    float getLayerOpacity(int layerIndex) const;
    int getLayerBlendMode(int layerIndex) const;
    bool isLayerActive(int layerIndex) const;
    bool isLayerConnected(int layerIndex) const;
    std::string getLayerSourceName(int layerIndex) const;
    
    // Parameter for GUI
    ofParameter<int> numActiveLayers;
    
    // Serialization - custom format for dynamic layers
    ofJson serialize() const override;
    void deserialize(const ofJson& json) override;
    
private:
    void onNumLayersChanged(int& count);      // Listener for numActiveLayers
    void resizeLayerArrays(int newSize);      // Resize parameter arrays
    
    int fboWidth = 1920;
    int fboHeight = 1080;
    int maxSupportedLayers = 64;              // Detected from GPU, default 64
    

    // Per-layer parameters (dynamically sized)
    std::vector<ofParameter<float>> layerOpacities;
    std::vector<ofParameter<int>> layerBlendModes;
    std::vector<ofParameter<bool>> layerActive;
};
