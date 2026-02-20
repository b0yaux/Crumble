# Crumble

Minimal modular video graph system for openFrameworks.

## Features

- **Live Scripting**: Rapid "jam-style" graph construction using Lua.
- **Reactive Architecture**: Parameters automatically trigger C++ actions (loading files, resizing arrays) via `ofParameter` listeners.
- **Recursive Modularity**: Support for nested sub-graphs and encapsulated components.
- **Directory Batching**: Easily import and wire entire folders of media via Lua.
- **HAP Integration**: Native support for HAP codec via `ofxHapPlayer`.

## Architecture

Crumble follows a **Reactive Pull-based Modular** design.

- **`Node`**: The base class. Nodes are self-contained, reactive processors.
- **`Graph`**: Manages nodes and connections. A `Graph` is itself a `Node`, enabling recursive nesting.
- **`ScriptBridge`**: Facilitates communication between Lua and C++.

### Directory Structure

```
src/
├── core/
│   ├── Node.h/cpp          # Base node & serialization logic
│   ├── Graph.h/cpp         # Graph & Recursive nesting engine
│   ├── ScriptBridge.h/cpp  # Lua bridge & DSL definition
│   └── Session.h/cpp       # High-level state management
└── nodes/
    └── video/
        ├── VideoFileSource.h/.cpp  # Reactive video player
        ├── VideoMixer.h/.cpp       # Auto-expanding blender
        └── ScreenOutput.h/.cpp     # Display sink
```

## Live Scripting DSL

### Basic Setup
```lua
clear()
local v = addNode("VideoFileSource", "V1")
v.videoPath = "path/to/video.mov"

local mixer = addNode("VideoMixer")
connect(v, mixer, 0, 0) -- Mixer auto-expands on connect
mixer.opacity_0 = 0.5
```

### Batch Importing
```lua
-- Automatically imports all .mov/.hap files and wires them to a mixer
local clips = importFolder("videos/loops")

for i, node in ipairs(clips) do
    connect(node, mixer, 0, i-1)
end
```

## Building

```bash
cd Crumble
make
```

## Shortcuts

- `G` : Toggle GUI
- `Cmd+Z` / `Cmd+Shift+Z` : Undo / Redo
- `S` : Save current graph to `main.json`
- Drag & Drop media files to auto-add as layers
