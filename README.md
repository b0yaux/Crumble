# Crumble

Minimal modular video graph system for openFrameworks.

## Features

- **Live Scripting**: Rapid "jam-style" graph construction using Lua.
- **Reactive Architecture**: Parameters automatically trigger C++ actions (loading files, resizing arrays) via `ofParameter` listeners.
- **Dynamic Blending**: Multi-layer video blending (64+ layers) with JIT parameter creation.
- **HAP Integration**: Native support for HAP codec via `ofxHapPlayer`.

## Architecture

Crumble follows a **Reactive Pull-based Modular** design.

- **`Node`**: The base class. Nodes are self-contained, reactive processors.
- **`Graph`**: Manages a collection of nodes and their connections. A `Graph` is itself a `Node`, allowing for nested sub-graphs.
- **`ScriptBridge`**: Facilitates communication between the Lua scripting environment and the Crumble C++ engine.
- **Reactive Hooks**: Nodes can override `onInputConnected` to dynamically adjust their internal state (like the `VideoMixer` expanding layers) when connections are made in real-time.

### Directory Structure

```
src/
├── core/
│   ├── Node.h/cpp          # Base node & serialization logic
│   ├── Graph.h/cpp         # Graph & Pull-evaluation engine
│   ├── ScriptBridge.h/cpp  # Lua bridge & DSL definition
│   └── Session.h/cpp       # High-level state management
```
src/
├── core/
│   ├── Node.h/cpp          # Base node & serialization logic
│   ├── Graph.h/cpp         # Graph & Pull-evaluation engine
│   ├── ScriptEngine.h/cpp  # Lua bridge & DSL definition
│   └── Session.h/cpp       # High-level state management
└── nodes/
    └── video/
        ├── VideoFileSource.h/.cpp  # Reactive video player
        ├── VideoMixer.h/.cpp       # Auto-expanding blender
        └── ScreenOutput.h/.cpp     # Display sink
```

## Live Scripting DSL

Nodes can be created and connected with a concise Lua syntax:

```lua
clear()
local v = addNode("VideoFileSource", "V1")
v.videoPath = "path/to/video.mov"

local mixer = addNode("VideoMixer")
connect(v, mixer, 0, 0) -- Mixer auto-expands on connect
mixer.opacity_0 = 0.5
```

## Building

```bash
cd Crumble
make
```

## Usage

- Drag `.mov` or `.hap` files onto the window to add video layers
- `+/-` : Add/Remove layers
- `1-8` : Select & toggle layer
- `[/]` : Adjust opacity
- `B` : Cycle blend mode (ALPHA → ADD → MULTIPLY)
- `G` : Toggle GUI
- `T` : Add 10 test layers

## Architecture

```
src/
├── core/
│   ├── Node.h          # Base node class
│   ├── Graph.h/.cpp    # Graph IS a Node, manages connections
│   └── PatchLoader.h/.cpp  # JSON patch loading (WIP)
└── nodes/
    └── video/
        ├── VideoFileSource.h/.cpp  # HAP video player
        ├── VideoMixer.h/.cpp       # Multi-layer blender
        └── ScreenOutput.h/.cpp     # Display sink
```

## Blend Modes

- **ALPHA**: Standard alpha blending (layer 1)
- **ADD**: Additive blending (layers 2, 4, ...)
- **MULTIPLY**: Multiply blending (layers 3, 5, ...)
