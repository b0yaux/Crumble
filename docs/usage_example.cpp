// Example: How to use the new Graph serialization and commands
// This file shows the recommended patterns for Crumble

#include "ofApp.h"
#include "Commands.h"

// ============================================================================
// SETUP: Register node types
// ============================================================================

void ofApp::setup() {
    // Register node types with the graph factory
    // This is done once during setup
    mainGraph.registerNodeType("VideoMixer", []() {
        return std::make_unique<VideoMixer>();
    });
    
    mainGraph.registerNodeType("VideoFileSource", []() {
        return std::make_unique<VideoFileSource>();
    });
    
    mainGraph.registerNodeType("ScreenOutput", []() {
        return std::make_unique<ScreenOutput>();
    });
    
    // Or load from a patch file
    if (ofFile::doesFileExist("patches/main.json")) {
        mainGraph.loadFromFile("patches/main.json");
    } else {
        // Create default graph programmatically
        createDefaultGraph();
    }
}

// ============================================================================
// CREATING NODES: Use factory instead of templates
// ============================================================================

void ofApp::createDefaultGraph() {
    // OLD WAY (still works for hardcoded graphs):
    // mixer = &mainGraph.addNode<VideoMixer>();
    
    // NEW WAY (factory-based, works with serialization):
    mixer = dynamic_cast<VideoMixer*>(mainGraph.createNode("VideoMixer", "MainMixer"));
    mixer->setup(1920, 1080);
    
    output = dynamic_cast<ScreenOutput*>(mainGraph.createNode("ScreenOutput", "Output"));
    
    // Create a video source
    auto* source = mainGraph.createNode("VideoFileSource", "Video1");
    
    // Connect nodes
    mainGraph.connect(source->nodeIndex, mixer->nodeIndex, 0, 0);
    mainGraph.setVideoOutputNode(output->nodeIndex);
}

// ============================================================================
// USING COMMANDS: For undo/redo support
// ============================================================================

CommandHistory history;

void ofApp::addVideoLayer(const std::string& filePath) {
    // Using commands enables undo/redo
    cmd::AddNode addCmd;
    addCmd.type = "VideoFileSource";
    addCmd.name = "Video" + std::to_string(mainGraph.getNodeCount());
    
    history.execute(addCmd, mainGraph);
    
    // The command stored the created index, use it to configure
    if (addCmd.createdIndex >= 0) {
        auto* source = dynamic_cast<VideoFileSource*>(mainGraph.getNode(addCmd.createdIndex));
        if (source) {
            source->load(filePath);
        }
        
        // Connect to mixer using another command
        cmd::Connect connectCmd;
        connectCmd.fromNode = addCmd.createdIndex;
        connectCmd.toNode = mixer->nodeIndex;
        connectCmd.toInput = mixer->getConnectedLayerCount();
        
        history.execute(connectCmd, mainGraph);
    }
}

void ofApp::keyPressed(int key) {
    // Undo/Redo support
    if (key == 'z' && ofGetKeyPressed(OF_KEY_COMMAND)) {
        if (ofGetKeyPressed(OF_KEY_SHIFT)) {
            history.redo(mainGraph);
        } else {
            history.undo(mainGraph);
        }
    }
    
    // Other shortcuts...
}

// ============================================================================
// SERIALIZATION: Save/Load entire graph
// ============================================================================

void ofApp::savePatch() {
    // Save current graph state
    mainGraph.saveToFile("patches/main.json");
    
    // Or get JSON for custom handling
    ofJson json = mainGraph.toJson();
    // Could send over network, store in memory, etc.
}

void ofApp::loadPatch(const std::string& path) {
    // Using command for undo support
    cmd::LoadPatch loadCmd;
    loadCmd.filePath = path;
    history.execute(loadCmd, mainGraph);
}

// ============================================================================
// MODIFYING PARAMETERS: With undo support
// ============================================================================

void ofApp::setLayerOpacity(int layer, float opacity) {
    // Using command for undo support
    cmd::SetParam setCmd;
    setCmd.nodeIndex = mixer->nodeIndex;
    setCmd.paramName = "layerOpacity" + std::to_string(layer);
    setCmd.newValue = opacity;
    
    history.execute(setCmd, mainGraph);
}

// ============================================================================
// JSON SCHEMA
// ============================================================================

/*
Example patch file (patches/main.json):

{
  "version": "1.0",
  "type": "Graph",
  "nodes": [
    {
      "id": 0,
      "type": "VideoMixer",
      "name": "MainMixer",
      "params": {
        "numLayers": 2
      }
    },
    {
      "id": 1,
      "type": "VideoFileSource",
      "name": "Video1",
      "params": {
        "videoPath": "videos/sample1.mov",
        "loop": true
      }
    },
    {
      "id": 2,
      "type": "ScreenOutput",
      "name": "Output"
    }
  ],
  "connections": [
    {
      "from": 1,
      "to": 0,
      "fromOutput": 0,
      "toInput": 0
    }
  ],
  "outputs": {
    "video": 2
  }
}

*/

// ============================================================================
// KEY BENEFITS OF THIS ARCHITECTURE
// ============================================================================

/*
1. MINIMAL CHANGES: 
   - Only 1 new file (Commands.h)
   - Enhanced existing Graph class
   - No separate Serializer or Factory classes

2. BACKWARD COMPATIBLE:
   - Template addNode<>() still works
   - Existing code doesn't break

3. SERIALIZATION:
   - Automatic via ofSerialize/ofDeserialize
   - Supports all ofParameter types
   - JSON format is human-readable

4. COMMANDS:
   - Simple structs, no inheritance
   - std::variant for type safety
   - Optional undo/redo support

5. EXTENSIBILITY:
   - Register new node types easily
   - Commands can be extended
   - JSON schema is versioned
*/
