# Crumble

Minimal modular video graph system for openFrameworks.

## Features

- **Live Scripting**: Rapid "jam-style" graph construction using Lua with idempotent reload.
- **Reactive Architecture**: Parameters automatically trigger C++ actions (loading files, resizing arrays) via `ofParameter` listeners.
- **Recursive Modularity**: Support for nested sub-graphs and encapsulated components.
- **Directory Batching**: Easily import and wire entire folders of video/audio media via Lua.
- **Asset Caching**: Automatic deduplication of audio files to prevent redundant RAM usage.
- **HAP Integration**: Native support for HAP codec via `ofxHapPlayer`.

## Architecture

Crumble follows a **Reactive Pull-based Modular** design.

- **`Node`**: The base class. Nodes are self-contained, reactive processors.
- **`Graph`**: Manages nodes and connections. A `Graph` is itself a `Node`, enabling recursive nesting.
- **`Session`**: High-level API surface with script lifecycle management.
- **`ScriptBridge`**: Facilitates communication between Lua and C++.
- **`AssetPool`**: Media caching and deduplication for audio resources.

### Directory Structure

```
src/
├── core/
│   ├── Node.h/cpp          # Base node & serialization logic
│   ├── Graph.h/cpp         # Graph & recursive nesting engine
│   ├── Session.h/cpp       # High-level API, script lifecycle
│   ├── ScriptBridge.h/cpp  # Lua bridge & DSL definition
│   └── AssetPool.h/cpp     # Audio caching & deduplication
├── nodes/
│   ├── video/
│   │   ├── VideoFileSource.h/.cpp  # HAP-based video player
│   │   ├── VideoMixer.h/.cpp       # Auto-expanding compositing mixer
│   │   └── ScreenOutput.h/.cpp     # Display sink
│   └── audio/
│       ├── AudioFileSource.h/.cpp  # RAM-cached audio playback
│       ├── AudioMixer.h/.cpp       # Reactive audio summation
│       └── SpeakersOutput.h/.cpp   # Audio output sink
├── ui/
│   └── GraphUI.h/.cpp     # Interactive graph visualization
└── crumble/
    └── Registry.h/cpp     # Node type factory registration
```

## Live Scripting DSL

### Basic Setup
```lua
clear()
local v = addNode("VideoFileSource", "V1")
v.path = "path/to/video.mov"

local mixer = addNode("VideoMixer")
connect(v, mixer, 0, 0) -- Mixer auto-expands on connect
mixer["opacity_0"] = 0.5
```

### Batch Importing
```lua
-- Automatically imports video (.mov/.hap/.mp4/.avi)
-- and audio (.wav/.mp3/.aif) files and wires them to a mixer
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
- `Cmd+S` : Save current graph to `main.json`
- Drag & Drop media files to auto-add as layers

## Configuration

`bin/data/config.json` controls physics and scripts:

```json
{
  "physics": { "damping": 0.85, "springStrength": 0.2, "repulsion": 500 },
  "entryScripts": ["main", "sources"]
}
```

## Recent Changes

- Fixed drag over-reactivity, lowered damping
- Added config.json for physics parameters and multiple script loading
- Added topological spawn positioning
