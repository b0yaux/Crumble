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

- **AudioProcessor**: High-performance DSP worker living on the audio thread, receiving commands via wait-free SPSC queues.
- **VideoProcessor**: GPU compositor evaluated on the main thread, using independent command queues for frame compositing.
- **Soft Sync**: Audio and video run naturally in parallel without hard seeks; ofxHapPlayer handles background decoding.
- **Patterns**: Stateless recipes (`cycle -> value`) used for sample-accurate modulation.
- **Interpreter**: The Lua runtime that parses and executes live-coding scripts.
- **AssetRegistry**: A logical mapping layer that handles media discovery, banks, and automatic A/V pairing.
- **Shadow Processors**: High-performance worker objects that live on the audio thread, receiving parameter changes via wait-free SPSC queues.
- **AssetCache**: A global registry that deduplicates media files and caches RAM buffers for efficiency.

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

### Graph Construction & Routing
Crumble supports **Auto-Indexing** and **Table Routing** for concise graph setup.
```lua
local sampler = addNode("AVSampler", "s1")
local vmix = addNode("VideoMixer", "vmix")
local amix = addNode("AudioMixer", "amix")

-- Route one source to multiple mixers. 
-- connect() finds the next free slot and returns the layer index.
local layer = connect(sampler, {vmix, amix})

vmix["opacity_" .. layer] = 0.5
amix["gain_" .. layer] = 0.5
```

### Sequencing & Modulation
Crumble features a stateless, sample-accurate math engine. You can compose complex modulators using functional operators:
```lua
local smp = addNode("AVSampler")

-- Composition: Mix a sequence with an LFO
smp.speed = seq("1 2 4") * osc(0.5)

-- Time Warping: Play a sequence at double speed
smp.speed = fast(2, seq("1 0.5 2 0.25"))

-- Generative Logic: Quantize a sine wave into 4 discrete steps
smp.volume = snap(4, osc(1.0))

-- Mapping: Scale a 0-1 LFO to a specific range (e.g. 200Hz to 2000Hz)
smp.cutoff = scale(200, 2000, osc(0.25))
```

#### Pattern Library

| Function | Description |
|----------|-------------|
| `osc(f)` | Sine wave (frequency in cycles-per-bar) |
| `ramp(f)` | Sawtooth (0.0 to 1.0) |
| `noise(s)`| Deterministic stochastic noise (optional seed) |
| `seq("...")`| Discrete step sequencer |
| `fast(n, p)`| Speed up pattern `p` by factor `n` |
| `slow(n, p)`| Slow down pattern `p` by factor `n` (1/n speed) |
| `shift(o, p)`| Offset phase by `o` (0.0 to 1.0) |
| `scale(l, h, p)`| Map pattern range to [low, high] |
| `snap(s, p)`| Quantize output into `s` steps |
| `p1 * p2` | Multiply two patterns (Amplitude Modulation) |
| `p1 + p2` | Add two patterns (Offset/Mixing) |

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
| **AV** | `AVSampler` | Unified audio+video player with Soft Sync |

## Robustness

- **Null-Safety**: Setting parameters to `nil` or passing `nil` to routing functions logs a warning without crashing the application.
- **State Preservation**: The C++ rendering engine remains active and maintains the last valid graph state when a Lua script encounters runtime errors.

## Shortcuts

| Key | Action |
|-----|--------|
| `G` | Toggle Graph UI |
| `Cmd+S` | Save current graph state to `main.json` |
