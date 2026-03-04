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
- **Data Flow**: Timing is **pushed** (frame-accurate sync), while data is **pulled** (efficient GPU/Audio demand).

## Architecture

```text
Session (Root Container)
├── Transport (Musical Clock & Phase)
├── Patterns (Stateless logic: cycle -> value shapes)
├── Graph (Recursive topology: the modular network)
│   └── Node (Atomic processing units)
├── Interpreter (Execution: Lua DSL runtime)
├── AssetRegistry (Logical VFS: Banks & Asset Pairing)
└── AssetCache (Efficiency: Deduplicated media & RAM storage)
```

## Media Management

Crumble features a **Logical Media Engine** that decouples scripts from physical file locations. Configure libraries in `config.json` via `searchPaths`.

### Unified Asset Loading
Load media into nodes using logical strings:
- **Bank Index**: `node.path = "drums:5"` (6th asset in the 'drums' folder)
- **Logical Name**: `node.path = "birds"` (Finds associated video and audio files named 'birds')
- **Direct Path**: `node.path = "clips/loop.mov"` (Standard file path resolution)

## Lua API

### Graph Construction
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
```lua
local smp = addNode("AVSampler")
smp.path = "birds" -- Automatically pairs birds.mov and birds.wav

smp.speed = seq("1 0.5 2 -1")
smp.volume = osc(0.5) * seq("1 0") 
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
