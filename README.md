# Crumble

Simple audio+video live-scriptable node-graph system built with openFrameworks and Lua.

## Quick Start

```bash
cd Crumble
make
make RunRelease     # or run executable
```

The app loads `bin/data/config.json` which specifies the entry script. By default it runs `scripts/main.lua`.

## How It Works

**Core concepts:**
- **Node**: A single processing unit (video player, mixer, output).
- **Graph**: A container holding nodes and their connections. **Graphs are Nodes**, enabling infinite recursion.
- **Session**: Manages the root graph lifecycle and script reloading.
- **Pull-Based Data Flow**: Sinks (Outputs) pull data from sources. Video uses GPU pointer passing (passive pull); Audio uses buffer filling (active fill).

## Architecture

```text
Session (Root Container)
├── Transport (Musical Clock & Phase)
├── Patterns (Stateless logic: cycle -> value shapes)
│   ├── Seq (Rhythmic step shapes)
│   ├── Osc (Continuous wave shapes)
│   └── Ramp (Linear phase shapes)
├── Graph (Recursive topology: the modular network)
│   └── Node (Atomic processing units)
│       ├── Parameters (Stateful control)
│       └── Modulators (Pattern assignments)
├── Interpreter (Execution: Lua DSL runtime)
└── AssetCache (Efficiency: Deduplicated media & RAM storage)
```

### Key Components

- **Node**: The base unit. Exposes parameters that can be driven by constant values or mathematical patterns.
- **Graph**: A recursive container. Because a Graph is also a Node, Crumble supports infinite nesting.
- **Pattern**: A stateless recipe (`cycle -> value`) used for sample-accurate modulation.
- **Session**: The owner of the hardware audio callback and the root graph.
- **Interpreter**: The Lua runtime that parses and executes live-coding scripts.
- **AssetCache**: A global registry that deduplicates media files and caches RAM buffers.


### Data Flow (Push-Pull)

1. **Push (Timing)**: The Session pushes the current `cycle` and `step` to all nodes. Nodes pre-calculate their `Modulators` into `Control` buffers.
2. **Pull (Data)**: The hardware output pulls audio from the graph. Nodes recursively request buffers from their inputs, applying modulated parameters at the sample level.

## Lua API

### Graph Construction

```lua
-- Create nodes: addNode(type, name?)
local video = addNode("VideoFileSource", "movie1")
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

### Sample-Accurate Sequencing

Crumble features a sample-accurate math expression engine for parameter modulation. Instead of static values, you can assign sequences or LFOs directly:

```lua
local smp = addNode("AVSampler", "mySampler")
smp.videoPath = "clips/loop.mov"
smp.audioPath = "clips/loop.wav"

-- Modulate speed and volume at audio rate
smp.speed = seq("1 0.5 2 -1")
smp.volume = osc(2.0) * seq("1 0") 
```

### Subgraph Composition

Create a subgraph by adding a `Graph` node. Set its `script` parameter to load a nested topology.

```lua
-- main.lua (parent graph)
local sub = addNode("Graph", "mySubgraph")
sub.script = "scripts/inner.lua"  -- Reactive: triggers loading into the node

local player = addNode("VideoFileSource")
player.path = "clips/bg.mov"

-- Connect parent nodes to subgraph inlets/outlets
connect(player, sub, 0, 0) -- player -> subgraph Inlet(index=0)
connect(sub, output, 0, 0) -- subgraph Outlet(index=0) -> ScreenOutput
```

```lua
-- inner.lua (subgraph script)
local inlet = addNode("Inlet")
inlet.index = 0 -- Matches the connection in parent

local outlet = addNode("Outlet")
outlet.index = 0 -- Exposes this node to parent output 0

local mixer = addNode("VideoMixer")
connect(inlet, mixer)
connect(mixer, outlet)
```

## Node Types

### Subgraph
| Type | Parameter | Description |
|------|-----------|-------------|
| `Graph` | `script` | Path to Lua script to populate this node's internal graph |
| `Inlet` | `index` | Input boundary - pulls from parent graph connection at `index` |
| `Outlet` | `index` | Output boundary - exposes internal signal to parent graph at `index` |

### Video
| Type | Description |
|------|-------------|
| `VideoFileSource` | High-performance HAP video player |
| `VideoMixer` | Multi-layer GPU compositor with reactive parameters |
| `ScreenOutput` | Renders a texture to the display |

### Audio
| Type | Description |
|------|-------------|
| `AudioFileSource` | RAM-cached sample player via AssetCache |
| `AudioMixer` | Multi-channel summation with per-input gains |
| `SpeakersOutput` | Hardware sink (SoundStream entry point) |

## Shortcuts

| Key | Action |
|-----|--------|
| `G` | Toggle GUI |
| `Cmd+S` | Save graph to `main.json` |
| Drag & Drop | Add media files as layers |
