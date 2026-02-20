#pragma once
#include "../core/Patch.h"
#include <string>

// Compound operations that coordinate multiple nodes.
// Single-node ops (opacity, blend, etc.) go directly to the node.
// This is the ONLY place that knows about specific node type interactions.

namespace crumble {

// Create a VideoFileSource, load the file, wire it to the next free mixer layer.
// Returns the mixer layer index, or -1 on failure.
int addVideoLayer(Patch& patch, const std::string& filePath);

// Remove a mixer layer: disconnect, shift connections, remove source node, remove layer.
// Safe to call with any index — does nothing if out of range or only 1 layer remains.
void removeLayer(Patch& patch, int layerIndex);

// Initialize a default Crumble patch: register node types, create Mixer + Output.
// Called once at startup. Separate from Patch::init() because it's Crumble-specific.
void initDefaultPatch(Patch& patch, int width, int height);

} // namespace crumble
