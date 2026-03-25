# Crumble

Simple audio+video live-scriptable node-graph system built with openFrameworks and Lua.

## Quick Start

```bash
cd Crumble
make
make RunRelease     # loads bin/data/config.json
```

### Command-Line Options

```bash
./Crumble                              # Default: config.json → entryScript
./Crumble -s scripts/drums.lua         # Override script
./Crumble -c drums.json                # Use different config file
./Crumble -t "Drums" -s drums.lua      # Set window title + script
```

| Flag | Description | Default |
|------|-------------|---------|
| `-c, --config` | Config file path | `config.json` |
| `-s, --script` | Override entry script | (from config) |
| `-t, --title` | Window title | (none) |

### Multi-Instance

Run multiple Crumble instances with different scripts:

```bash
# Terminal 1
./Crumble -s scripts/drums.lua -t "Drums"

# Terminal 2
./Crumble -s scripts/melody.lua -t "Melody"
```

## How It Works

- **Node**: A single processing unit (video player, mixer, output).
- **Graph**: A recursive container. **Graphs are Nodes**, enabling infinite recursion.
- **Session**: Manages the root graph lifecycle and hardware audio/video streams.

## Architecture

Crumble uses a **Shadow Processor** architecture to decouple slow UI/Lua logic from the real-time audio and video threads.

```text
Session (Root Container: Hardware & Threading)
├── Transport (Musical Clock & Phase)
├── AudioThread: AudioProcessor (Wait-free Audio DSP)
├── MainThread: VideoProcessor (GPU Compositing)
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
- **Shadow Processors**: Internal high-performance workers that decouple C++ processing from Lua. They use wait-free SPSC queues and lock-free memory arrays to guarantee glitch-free audio during live-coding.

## Data Types & Flows

Crumble uses a two-phase **Push-Pull** model to ensure sample-accurate sync between mathematical patterns, high-fidelity audio, and GPU-accelerated video.

### 1. The Push Phase (Sync & Control)
The Session pushes a **Timing Context** to all nodes at the start of every hardware block.
- **Type (`Control`)**: A vectorized block of `float` values (**K-rate**).
- **The Flow**: Nodes pre-calculate their mathematical `Patterns` into high-speed buffers.
- **The Potential**: This **bridges the gap** between slow Lua logic and fast C++ DSP. It enables **Sample-Accurate Modulation**, where a parameter (like `speed` or `filter`) can change its value for every single audio sample.

### 2. The Pull Phase (Signal Processing)
Data is pulled through the graph only when an output device (Speakers/Screen) demands it.
- **Audio (`ofSoundBuffer`)**: Multi-channel **floating-point PCM**. The hardware thread recursively requests nodes to fill the buffer (**Active Fill**), performing high-fidelity signal summation in real-time.
- **Video (`ofTexture*`)**: Pointers to **GPU-resident textures**. Sinks pull data from their sources on-demand (**Passive Pull**). This "Zero-Copy" flow enables high-performance mixing by referencing GPU memory rather than copying pixel data.

## Media Management

Crumble features a **Logical Media Engine** that decouples scripts from physical file locations. Configure libraries in `config.json` via `searchPaths`.

### Unified Asset Loading
Load media into nodes using logical strings:
- **Bank Index**: `node.path = "drums:5"` (6th asset in the 'drums' folder).
- **Logical Name**: `node.path = "birds"` (Finds associated media files named 'birds').
- **Direct Path**: `node.path = "clips/loop.mov"` (Standard file path resolution).

### Data-Driven Scripts
Query the `AssetRegistry` directly to build generative graphs based on folder contents:
```lua
local assets = getBank("my-folder")
for i, asset in ipairs(assets) do
    local s = addNode("AVSampler", asset.name)
    s.path = asset.path -- e.g. "my-folder:0"
end
```

## Lua API

### Declarative Construction
Crumble supports a modern, table-based declarative syntax. You can initialize a node and all its parameters in a single block.

```lua
-- Declarative Syntax (Recommended)
local smp = sampler("s1", { 
    path = "drums:0", 
    gain = osc(0.5):scale(0.5, 1.0) 
})

-- Method Chaining
local v = videomix("mix"):opacity(0.8):on()

-- Full Chained Pipeline
local a = audiomix("mix"):gain(0.5):connect(audioout)
```

### Graph Construction & Routing
Crumble supports **Auto-Indexing** and **Table Routing** for concise graph setup.
```lua
-- Route one source to multiple mixers. 
-- connect() finds the next free slot and returns the layer index.
local layer = smp:connect({v, a})

v["opacity_" .. layer] = 0.5
a["gain_" .. layer] = 0.5
```


### Sequencing & Modulation
Crumble features a stateless, sample-accurate math engine. You can compose complex modulators using functional operators or method chaining:

```lua
-- Composition: Mix a sequence with an LFO
smp.speed = seq("1 2 4") * osc(0.5)

-- Time Warping: Play a sequence at double speed
smp.speed = seq("1 0.5 2 0.25"):fast(2)
```


#### Pattern Library

| Function | Description |
|----------|-------------|
| `osc(f)` | Sine wave (frequency in cycles-per-bar) |
| `ramp(f)` | Sawtooth (0.0 to 1.0, frequency in cycles-per-bar) |
| `noise(f, s)`| Smooth dynamic modulation (freq, seed) |
| `rand(s)` | Static deterministic random number (seed) |
| `seq("...")`| Discrete step sequencer |

#### Chaining Methods

| Method | Description |
|--------|-------------|
| `.fast(n)`| Speed up pattern by factor `n` |
| `.slow(n)`| Slow down pattern by factor `n` (1/n speed) |
| `.shift(o)`| Offset phase by `o` (0.0 to 1.0) |
| `.scale(l, h)`| Map pattern range to [low, high] |
| `.snap(s)`| Quantize output into `s` steps |

> **Timing contract:** All pattern frequencies are in **cycles per bar**.
> Patterns use **Monotonic Phase**: they do NOT reset at bar boundaries,
> enabling ultra-slow modulations (e.g. `osc(0.01)` completes over 100 bars).

### Tempo & Clock

Control the master transport from Lua using standard live-coding aliases. By default, **1 Cycle = 4 Beats**.

| Function | Unit | Formula | Example |
|----------|------|---------|---------|
| `bpm(x)` | Beats Per Minute | `BPM = x` | `bpm(120)` |
| `cpm(x)` | Cycles Per Minute | `BPM = x * 4` | `cpm(30)` |
| `cps(x)` | Cycles Per Second | `BPM = x * 240` | `cps(0.5)` |

The global `Time` table provides real-time access to the transport:

| Key | Unit | Description |
|-----|------|-------------|
| `Time.abs` | Seconds | Absolute running time |
| `Time.bars`| Cycles | Total bars elapsed (monotonic) |
| `Time.cycle`| 0.0–1.0 | Current bar phase (wrapped) |
| `Time.tempo`| BPM | Current beats-per-minute |

### Subgraph Composition
Graphs are recursive: a `sub` node can contain its own nested graph, loaded from a script.
```lua
local g = sub("drums", { script = "inner.lua" })
```

#### Inlet/Outlet Boundary Nodes
Subgraphs use special boundary nodes to connect to their parent:
```lua
-- scripts/inner.lua
local inNode = inlet("in")         -- Receives from parent
local s = sampler("s1")
local outNode = outlet("out")      -- Exposes to parent

connect(inNode, s)
connect(s, outNode)
```

In the parent graph, connect to the subgraph as if it were any other node:
```lua
local g = sub("s", { script = "inner.lua" })
connect(sampler("src"), g)  -- Routes through boundary nodes
```


### Module System
Lua's `require()` is available for code organization:
```lua
-- scripts/utils.lua
local M = {}
M.makeMixer = function(name)
    return addNode("AudioMixer", name)
end
return M

-- scripts/main.lua
local utils = require("utils")
local mix = utils.makeMixer("mainMix")
```

> **Note:** All required modules share the same global namespace. Node names must be unique across all loaded modules.

### Live Reload Behavior

| Trigger | Behavior |
|---------|----------|
| `.lua` file saved | Hot-reload: existing nodes keep state, new nodes created, removed nodes deleted |
| `entryScript` changed in `config.json` | Full reset: graph cleared, new script starts fresh |
| `config.json` saved | Reload configuration |

This enables stable live-coding: editing the current script preserves playback state, while switching to a different script provides a clean slate.

## Node Reference

All nodes inherit these base parameters:

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `gain` | float | 0.0–4.0 | Amplitude scalar (audio). Follows Tidal/Strudel/ChucK convention. Values above 1.0 boost above unity. |
| `opacity` | float | 0.0–1.0 | Transparency scalar (video). Follows Hydra convention. |
| `active` | bool | — | On/off bypass. When `false`, audio outputs silence and video outputs `nil`. |
| `drawLayer` | int | -100–100 | Render order for drawable nodes (e.g. ScreenOutput). |

| Category | Type | Description |
|----------|------|-------------|
| **Core** | `Graph` | Nested scriptable sub-graph |
| **Video** | `VideoFileSource` | High-performance HAP video player |
| | `VideoMixer` | Multi-layer GPU compositor |
| | `ScreenOutput` | Renders a texture to the display |
| **Audio** | `AudioFileSource` | RAM-cached sample player |
| | `AudioMixer` | Multi-channel summation |
| | `SpeakersOutput` | Hardware audio sink |
| **AV** | `AVSampler` | Unified audio+video player with parallel playback |

## Robustness

- **Null-Safety**: Setting parameters to `nil` or passing `nil` to routing functions logs a warning without crashing the application.
- **State Preservation**: The C++ rendering engine remains active and maintains the last valid graph state when a Lua script encounters runtime errors.

## Shortcuts

| Key | Action |
|-----|--------|
| `G` | Toggle Graph UI |
| `Cmd+S` | Save current graph state to `main.json` |
