# Crumble

Simple audio+video live-scriptable node-graph system built with openFrameworks and Lua.

## Quick Start

```bash
cd Crumble
make
make RunRelease     # loads bin/data/config.json
```

## How It Works

- **Node**: A single processing unit (video player, mixer, output).
- **Graph**: A recursive container. **Graphs are Nodes**, enabling infinite recursion.
- **Session**: Manages the root graph lifecycle and hardware audio/video streams.

## Architecture

```text
Session (Root Container: Hardware & Threading)
├── Transport (Musical Clock & Phase)
├── Patterns (Stateless logic: cycle -> value shapes)
├── Graph (Recursive topology & Node lifecycle)
│   └── Node (Atomic processing units)
│       ├── Parameters (Stateful control)
│       └── Modulators (Pattern assignments)
├── Interpreter (Lua DSL & Bindings)
├── AssetRegistry (Logical VFS: Banks & Asset discovery)
└── AssetCache (Deduplicated RAM storage)
```

### Key Components

- **Patterns**: Stateless recipes (`cycle -> value`) used for sample-accurate modulation.
- **Interpreter**: The Lua runtime that parses and executes live-coding scripts.
- **AssetRegistry**: A logical mapping layer that handles media discovery, banks, and automatic A/V pairing.
- **AssetCache**: A global registry that deduplicates media files and caches RAM buffers for efficiency.

### Data Flow (Push-Pull)

1. **Push (Timing)**: The Session pushes the current `cycle` and `step` to all nodes. Nodes pre-calculate their `Modulators` into vectorized `Control` buffers.
2. **Pull (Data)**: The hardware output pulls data from the graph. Video uses GPU pointer passing (passive pull); Audio uses buffer filling (active fill).

## Media Management

Crumble features a **Logical Media Engine** that decouples scripts from physical file locations. Configure libraries in `config.json` via `searchPaths`.

### Unified Asset Loading
Load media into nodes using logical strings:
- **Bank Index**: `node.path = "drums:5"` (6th asset in the 'drums' folder).
- **Logical Name**: `node.path = "birds"` (Finds associated media files named 'birds').
- **Direct Path**: `node.path = "clips/loop.mov"` (Standard file path resolution).

## Lua API

### Graph Construction & Routing
```lua
local video = addNode("VideoFileSource", "movie1")
local mixer = addNode("VideoMixer")
local output = addNode("ScreenOutput")

connect(video, mixer)
connect(mixer, output)

video.path = "superstratum:40" 
mixer.opacity_0 = 0.5
```

### Sequencing & Modulation
Crumble features a sample-accurate math engine. You can assign sequences, LFOs, or combine them using math operators:
```lua
local smp = addNode("AVSampler")
smp.path = "birds" -- Auto-pairs related files

smp.speed = seq("1 0.5 2") * 0.5     -- Scale a sequence
smp.volume = osc(0.5) + seq("0 0.5") -- Add an LFO offset
```

### Subgraph Composition
Create a subgraph by adding a `Graph` node and setting its `script` parameter:
```lua
local sub = addNode("Graph", "mySubgraph")
sub.script = "scripts/inner.lua" -- Populates the sub-graph reactively
```

## Node Reference

| Category | Type | Description |
|----------|------|-------------|
| **Core** | `Graph` | Nested scriptable sub-graph |
| **Video** | `VideoFileSource` | High-performance HAP video player |
| | `VideoMixer` | Multi-layer GPU compositor |
| | `ScreenOutput` | Renders a texture to the display |
| **Audio** | `AudioFileSource` | RAM-cached sample player |
| | `AudioMixer` | Multi-channel summation |
| | `SpeakersOutput` | Hardware audio sink |

## Shortcuts

| Key | Action |
|-----|--------|
| `G` | Toggle Graph UI |
| `Cmd+S` | Save current graph state to `main.json` |
| `Drag & Drop` | Add media files directly to the session |
