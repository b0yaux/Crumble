# Crumble

Simple audio+video live-scriptable node-graph system built with openFrameworks and Lua.

## Requirements

### macOS
```bash
# Install dependencies
brew install ffmpeg sdl2

# Clone Crumble into your openFrameworks apps folder
cd /path/to/openFrameworks/apps/myApps
git clone https://github.com/YOUR_USERNAME/Crumble.git

# Install ofxHapPlayer (uses system ffmpeg)
cd /path/to/openFrameworks/addons
git clone https://github.com/b0yaux/ofxHapPlayer.git

# Build
cd Crumble
make Release
make RunRelease
```

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
- **Shadow Processors**: Internal high-performance workers that decouple C++ processing from Lua, using wait-free SPSC queues.

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
    local s = addNode("sampler", asset.name)
    s.path = asset.path -- e.g. "my-folder:0"
end
```

## Lua API

### Graph Construction & Routing
Crumble supports **Auto-Indexing** and **Table Routing** for concise graph setup.
```lua
local sampler = addNode("sampler", "s1")
local vmix = addNode("videomix", "vmix")
local amix = addNode("audiomix", "amix")

-- Route one source to multiple mixers. 
-- connect() finds the next free slot and returns the layer index.
local layer = connect(sampler, {vmix, amix})

vmix["opacity_" .. layer] = 0.5
amix["gain_" .. layer] = 0.5
```

### Sequencing & Modulation
Crumble features a stateless, sample-accurate math engine. You can compose complex modulators using functional operators:
```lua
local smp = addNode("sampler")

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
| `osc(f)` | Sine wave (frequency `f` in cycles-per-bar) |
| `ramp(f)` | Sawtooth (0.0 to 1.0, frequency `f` in cycles-per-bar) |
| `noise(f, s)`| Deterministic stochastic noise (frequency `f`, optional seed `s`) |
| `seq("...")`| Discrete step sequencer |
| `fast(n, p)`| Speed up pattern `p` by factor `n` |
| `slow(n, p)`| Slow down pattern `p` by factor `n` (1/n speed) |
| `shift(o, p)`| Offset phase by `o` (0.0 to 1.0) |
| `scale(l, h, p)`| Map pattern range to [low, high] |
| `snap(s, p)`| Quantize output into `s` steps |
| `p1 * p2` | Multiply two patterns (Amplitude Modulation) |
| `p1 + p2` | Add two patterns (Offset/Mixing) |

> **Timing contract:** All pattern frequencies are in **cycles per bar**.
> `Transport.cycle` advances at `bpm / beatsPerBar` beats-per-second, wrapping
> once per bar. The default is `beatsPerBar = 4` (common time). Change it for
> other time signatures:
>
> | `beatsPerBar` | Time sig | Bar length at 120 BPM | `osc(1.0)` rate |
> |---|---|---|---|
> | 4 (default) | 4/4 | 2.0 s | 0.5 Hz |
> | 3 | 3/4 | 1.5 s | 0.67 Hz |
> | 5 | 5/4 | 2.5 s | 0.4 Hz |
>
> To modulate at beat rate in 4/4, use `fast(4, osc(1.0))` or simply `osc(4.0)`.

### Subgraph Composition
Graphs are recursive: a `graph` node can contain its own nested graph, loaded from a script.
```lua
local g = graph("mySubgraph", { script = "scripts/inner.lua" })
```

#### Inlet/Outlet Boundary Nodes
Subgraphs use special boundary nodes to connect to their parent:
```lua
-- scripts/inner.lua
local inlet = addNode("inlet", "in")      -- Receives from parent
local proc = addNode("audiomix", "mix")
local outlet = addNode("outlet", "out")   -- Exposes to parent

connect(inlet, proc)
connect(proc, outlet)
```

In the parent graph, connect to the subgraph as if it were any other node:
```lua
local src = addNode("audio", "src")
local g = graph("sub", { script = "scripts/inner.lua" })
connect(src, g)  -- Routes through Inlet/Outlet boundaries
```

### Module System
Lua's `require()` is available for code organization:
```lua
-- scripts/utils.lua
local M = {}
M.makeMixer = function(name)
    return addNode("audiomix", name)
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

| Category | Type | Alias | Description |
|----------|------|-------|-------------|
| **Core** | `Graph` | `graph` | Nested scriptable sub-graph |
| | `Inlet` | `inlet` | Sub-graph input boundary |
| | `Outlet` | `outlet` | Sub-graph output boundary |
| **Video** | `VideoSource` | `video` | High-performance HAP video player |
| | `VideoMixer` | `videomix` | Multi-layer GPU compositor |
| | `VideoOutput` | `videoout` | Master video sink |
| **Audio** | `AudioSource` | `audio` | RAM-cached sample player |
| | `AudioMixer` | `audiomix` | Multi-channel summation |
| | `AudioOutput` | `audioout` | Master audio sink |
| **AV** | `AVSampler` | `sampler` | Unified audio+video player |

## Robustness

- **Null-Safety**: Setting parameters to `nil` or passing `nil` to routing functions logs a warning without crashing the application.
- **State Preservation**: The C++ rendering engine remains active and maintains the last valid graph state when a Lua script encounters runtime errors.

## Requirements

### macOS
```bash
brew install ffmpeg sdl2
cd /path/to/openFrameworks/addons
git clone https://github.com/b0yaux/ofxHapPlayer.git
```

Uses system ffmpeg from Homebrew. The ofxHapPlayer fork includes the macOS fix.

## Shortcuts

| Key | Action |
|-----|--------|
| `G` | Toggle Graph UI |
| `Cmd+S` | Save current graph state to `main.json` |

## Hardware Input

Crumble supports real-time modulation from MIDI, OSC, and gamepad controllers. Allinput values are normalized to 0.0-1.0.

### MIDI

```lua
-- Control Change (knobs, faders)
s1:gain(midi(74, 1))           -- CC74 on channel 1

-- Note velocity (pad strike intensity)
s1:opacity(midinote(36, 10))   -- Note 36 on channel 10

-- Channel Aftertouch (keyboard pressure)
s1:speed(channeltouch(1):scale(0.5, 2.0))
```

### OSC

```lua
-- Listen on port 8000 (default)
s1:cutoff(oscin("/filterCutoff"))
```

### Gamepad

```lua
-- Semantic button names (Xbox/DualSense layout)
s1:gain(gpad("a"))             -- A button
s1:mute(gpad("lb"))             -- Left bumper

-- Analog axes
s1:speed(gax("ly"):scale(0.5, 2.0))  -- Left stick Y

-- Available constants:
-- GPAD: A, B, X, Y, LB, RB, BACK, START, GUIDE, LS, RS, UP, DOWN, LEFT, RIGHT
-- AXIS: LX, LY, RX, RY, LT, RT
```

### Pattern Composition

All hardware inputs return patterns that can be composed:

```lua
-- Combine MIDI CC and note velocity
local mix = midi(82, 1) + midinote(36, 10)
s1:gain(mix)

-- Scale and transform
s1:speed(gax("rx"):scale(-2, 2))-- Right stick X mapped to speed
```
