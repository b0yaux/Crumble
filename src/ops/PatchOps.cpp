#include "PatchOps.h"
#include "../nodes/video/VideoFileSource.h"
#include "../nodes/video/VideoMixer.h"
#include "../nodes/video/ScreenOutput.h"

namespace crumble {

void initDefaultPatch(Patch& patch, int width, int height) {
    // Register all node types
    patch.registerNodeType("VideoMixer", []() {
        return std::make_unique<VideoMixer>();
    });
    patch.registerNodeType("VideoFileSource", []() {
        return std::make_unique<VideoFileSource>();
    });
    patch.registerNodeType("ScreenOutput", []() {
        return std::make_unique<ScreenOutput>();
    });

    // Create default topology: Mixer -> Output
    auto* mixer = dynamic_cast<VideoMixer*>(patch.addNode("VideoMixer", "Mixer"));
    mixer->setup(width, height);
    mixer->setLayerCount(1);

    auto* output = dynamic_cast<ScreenOutput*>(patch.addNode("ScreenOutput", "Output"));
    output->setup(0, 0, ofGetWidth(), ofGetHeight());

    patch.connect(0, 1);  // Mixer -> Output
    patch.getGraph().setVideoOutputNode(0);
}

int addVideoLayer(Patch& patch, const std::string& filePath) {
    auto* mixer = patch.findFirstNodeOfType<VideoMixer>("VideoMixer");
    if (!mixer) return -1;

    // Create source node
    auto* source = dynamic_cast<VideoFileSource*>(patch.addNode("VideoFileSource"));
    if (!source) return -1;

    int sourceIdx = patch.getNodeCount() - 1;

    if (!filePath.empty()) {
        source->load(filePath);
        source->play();
    }

    // Find empty slot or add new layer
    int layerIdx = -1;
    for (int i = 0; i < mixer->getLayerCount(); i++) {
        if (!mixer->isLayerConnected(i)) {
            layerIdx = i;
            break;
        }
    }

    if (layerIdx < 0) {
        layerIdx = mixer->addLayer();
    }

    if (layerIdx >= 0) {
        patch.connect(sourceIdx, mixer->nodeIndex, 0, layerIdx);
    }

    return layerIdx;
}

void removeLayer(Patch& patch, int layerIndex) {
    auto* mixer = patch.findFirstNodeOfType<VideoMixer>("VideoMixer");
    if (!mixer) return;

    int currentCount = mixer->getLayerCount();
    if (currentCount <= 1) return;
    if (layerIndex < 0 || layerIndex >= currentCount) return;

    // 1. Find source node connected to this layer
    Node* sourceNode = mixer->getLayerSource(layerIndex);
    int sourceNodeIdx = sourceNode ? sourceNode->nodeIndex : -1;

    // 2. Disconnect this input and shift higher inputs down (atomic)
    patch.removeInput(mixer->nodeIndex, layerIndex);

    // 3. Remove source node from graph (safe: connections already cleaned up)
    if (sourceNodeIdx >= 0) {
        patch.removeNode(sourceNodeIdx);
    }

    // 4. Remove the mixer layer (shift parameter arrays)
    mixer->removeLayer(layerIndex);
}

} // namespace crumble
