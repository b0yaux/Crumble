# Crumble

A minimal modular video + audio graph system built with openFrameworks and Lua.

## Quick Start

```bash
cd Crumble
make
make RunRelease     # or run executable
```

The app loads `bin/data/config.json` which specifies the entry script. By default it runs `scripts/main.lua`.

## How It Works

**Core concepts:**
- **Node**: A single processing unit (video player, mixer, output)
- **Graph**: Container holding nodes and their connections
- **Session**: Manages the graph lifecycle and script reloading

Data flows from sources → mixers → outputs.

## Architecture

| Component | Role |
|-----------|------|
| `Node` | Base class for all processing units |
| `Graph` | Manages nodes + connections; recursively nestable |
| `Session` | Script lifecycle, reload, asset management |
| `ScriptBridge` | Lua ↔ C++ communication |
| `AssetPool` | Audio RAM deduplication |
| `FileWatcher` | Live script reload on file change |

## File Tree

```
src/
├── main.cpp, ofApp.cpp     # Entry point
├── core/
│   ├── Node.h/.cpp         # Base node, serialization
│   ├── Graph.h/.cpp        # Connections, recursive nesting
│   ├── Session.h/.cpp     # Script lifecycle
│   ├── ScriptBridge.h/.cpp # Lua DSL
│   ├── AssetPool.h         # Audio cache
│   ├── FileWatcher.h       # File change monitoring
│   └── Config.h/.cpp       # Runtime config
├── nodes/
│   ├── video/              # VideoFileSource, VideoMixer, ScreenOutput
│   └── audio/              # AudioFileSource, AudioMixer, SpeakersOutput
├── ui/GraphUI.cpp          # Graph visualization
└── crumble/Registry.cpp    # Node factory
```

## Lua API

### Graph Construction

```lua
-- Create nodes: addNode(type, name?)
local video = addNode("VideoFileSource", "myVideo")
local mixer = addNode("VideoMixer")
local output = addNode("ScreenOutput")

-- Connect: connect(from, to, fromOutput?, toInput?)
connect(video, mixer, 0, 0)
connect(mixer, output)

-- Set parameters directly
video.path = "clips/loop.mov"
mixer.opacity_0 = 0.5
mixer.blend_0 = 1  -- ADD blend mode
```

### Parameters

VideoMixer auto-expands when you connect inputs:
```lua
mixer.opacity_0 = 0.5   -- first input opacity
mixer.opacity_1 = 0.3   -- second input opacity
mixer.blend_0 = 1       -- blend mode (0=normal, 1=ADD)
```

### Recursive Scripting 'Lua loads Lua'

Use `require()` to use a lua script inside another.

/!\ To be verified : generic `loader` lua module:

```lua
local loader = require("loader")

-- Returns { ".ext": [{name, path, ext}, ...], ... }
local data = loader.scan("path/to/media", { limit = 32 })

local videos = data[".mov"] or {}
for i, v in ipairs(videos) do
    local node = addNode("VideoFileSource", "v" .. (i-1) .. "_" .. v.name)
    node.path = v.path
    connect(node, mixer, 0, i-1)
    mixer["opacity_" .. (i-1)] = 1.0 / #videos
end
```

### Utilities

```lua
clear()           -- Reset graph
listDir("path")   -- Returns table of file paths
fileExists("path") -- Boolean check
```

## Node Types

### Subgraph
| Type | Description |
|------|-------------|
| `Inlet` | Input boundary - pulls from parent graph connections |
| `Outlet` | Output boundary - exposes internal node output to parent |

## Subgraph Composition

Create a subgraph by adding a `Graph` node and setting its `script` parameter:

```lua
-- main.lua (parent graph)
local sub = addNode("Graph", "mySubgraph")
sub.script = "scripts/inner.lua"  -- Loads this script into the subgraph

local parentVideo = addNode("VideoFileSource", "parentVideo")
parentVideo.path = "clips/intro.mov"

-- Connect to subgraph: data flows through Inlet/Outlet
connect(parentVideo, sub, 0, 0)  -- → subgraph's Inlet
connect(sub, 0, 0)               -- subgraph's Outlet → ScreenOutput
```

```lua
-- inner.lua (subgraph script)
-- Inside subgraph: add Inlet/Outlet boundaries
local inlet = addNode("Inlet", "in")
local outlet = addNode("Outlet", "out")

-- Add internal nodes
local video = addNode("VideoFileSource", "innerVideo")
video.path = "clips/loop.mov"

local mixer = addNode("VideoMixer")

-- Wire: Inlet → mixer ← video → Outlet
connect(inlet, mixer, 0, 0)
connect(video, mixer, 0, 1)
connect(mixer, outlet, 0, 0)
```

The `Inlet` node pulls from parent graph connections, and `Outlet` exposes internal output to the parent.

### Video
| Type | Description |
|------|-------------|
| `AudioFileSource` | RAM-cached audio player |
| `AudioMixer` | Reactive audio summation |
| `SpeakersOutput` | Audio output sink |

## Configuration

Edit `bin/data/config.json`:

```json
{
  "entryScript": "scripts/main.lua",
  "layout": {
    "spawnPosition": "topological"
  },
       "physics": {
    "damping": 0.85,
    "springStrength": 0.2
  }
}
```

- **entryScript**: Path to your main Lua script
- **layout.spawnPosition**: `topological` or `center`
- **physics**: Graph layout simulation parameters

## Shortcuts

| Key | Action |
|-----|--------|
| `G` | Toggle GUI |
| `Cmd+S` | Save graph to `main.json` |
| Drag & Drop | Add media files as layers |
