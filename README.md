# Crumble

Simple audio+video live-scriptable node-graph system built with openFrameworks and Lua.

## Quick Start

```bash
cd Crumble
make
make RunRelease     # Loads bin/data/scripts/main.lua by default
```

## Core Concepts

- **Nodes**: Atomic processing units (Players, Mixers, Outputs).
- **Graphs**: Recursive containers. A Graph is itself a Node, allowing infinite nesting.
- **Data Flow**: Timing is **pushed** (frame-accurate sync), while data is **pulled** (efficient GPU/Audio demand).
- **Asset Engine**: A logical VFS that handles discovery, banks, and automatic A/V pairing.

## Media Library

Configure your libraries in `bin/data/config.json` via `searchPaths`. Crumble indexes these folders into logical **Banks** at startup.

### Loading Assets
Load media into nodes like `AVSampler` or `VideoFileSource` using unified strings:
- **Bank Index**: `node.path = "superstratum:40"` (Load the 41st asset in the 'superstratum' folder).
- **Logical Name**: `node.path = "kick_01"` (Finds associated video and audio files named 'kick_01' in any search path).
- **Direct Path**: `node.path = "clips/loop.mov"` (Standard relative or absolute file path).

## Lua API

### Graph Construction & Routing
```lua
-- Create and connect
local s1 = addNode("AVSampler", "sampler1")
local mixer = addNode("VideoMixer")
local out = addNode("ScreenOutput")

connect(s1, mixer)
connect(mixer, out)

-- Set logical assets
s1.path = "drums:5" -- Automatically pairs .mov and .wav
mixer.opacity_0 = 0.8
```

### Sequencing & Modulation
Crumble features a sample-accurate math engine for parameters:
```lua
-- Patterns: seq(), osc(), ramp()
s1.speed = seq("1 0.5 2 -1")
s1.volume = osc(0.5) * seq("1 0")
```

## Node Reference

| Category | Type | Description |
|----------|------|-------------|
| **Core** | `Graph` | Nested scriptable sub-graph |
| **Video** | `VideoFileSource` | HAP video player |
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
