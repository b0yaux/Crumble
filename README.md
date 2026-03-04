# Crumble

Simple audio+video live-scriptable node-graph system built with openFrameworks and Lua.

## Quick Start

```bash
cd Crumble
make
make RunRelease     # Loads bin/data/config.json
```

## How It Works

**Core concepts:**
- **Node**: A single processing unit (video player, mixer, output).
- **Graph**: A recursive container. **Graphs are Nodes**, enabling infinite recursion.
- **Session**: Manages the root graph lifecycle and script reloading.
- **Pull-Based Data Flow**: Sinks (Outputs) pull data from sources. Video uses GPU pointer passing (passive pull); Audio uses buffer filling (active fill).

## Architecture

```text
Session (Root Container)
├── Transport (Musical Clock & Phase)
├── Patterns (Stateless logic: cycle -> value shapes)
├── Graph (Recursive topology: the modular network)
│   └── Node (Atomic processing units)
│       ├── Parameters (Stateful control)
│       └── Modulators (Pattern assignments)
├── Interpreter (Execution: Lua DSL runtime)
├── AssetRegistry (Logical VFS: Banks & Asset Pairing)
└── AssetCache (Efficiency: Deduplicated media & RAM storage)
```

## Media Management

Crumble features a **Logical Media Engine** that decouples your scripts from physical file locations. Configure your libraries in `config.json` via `searchPaths`.

### Unified Asset Loading
Load media into nodes using logical strings:
- **Bank Index**: `node.path = "drums:5"` (6th asset in the 'drums' folder).
- **Logical Name**: `node.path = "kick_01"` (Finds associated media files named 'kick_01').
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
Crumble features a sample-accurate math expression engine for parameter modulation:
```lua
local smp = addNode("AVSampler")
smp.path = "birds" -- Automatically pairs birds.mov and birds.wav

smp.speed = seq("1 0.5 2 -1")
smp.volume = osc(0.5) * seq("1 0") 
```

## Node Types

| Category | Type | Description |
|----------|------|-------------|
| **Core** | `Graph` | Nested scriptable sub-graph |
| **Video** | `VideoFileSource` | High-performance HAP video player |
| | `VideoMixer` | Multi-layer GPU compositor |
| | `ScreenOutput` | GL Texture sink |
| **Audio** | `AudioFileSource` | RAM-cached sample player |
| | `AudioMixer` | Multi-channel summation |
| | `SpeakersOutput` | Hardware audio sink |

## Shortcuts

| Key | Action |
|-----|--------|
| `G` | Toggle Graph UI |
| `Cmd+S` | Save current graph state to `main.json` |
| `Drag & Drop` | Add media files directly to the session |
