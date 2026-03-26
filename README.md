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
- **Deterministic Reloads**: Auto-naming counters reset on every script execution, ensuring stable node identity during live-coding.

## Architecture

Crumble uses a **Shadow Processor** architecture to decouple slow UI/Lua logic from the real-time audio and video threads.

```text
Session (Root Context)
├── AudioThread: AudioProcessor (Wait-free DSP)
├── MainThread: VideoProcessor (GPU Compositing)
├── Interpreter (Lua DSL & Alias-Aware Auto-Naming)
├── Registry (Unified Node Factory)
├── Graph (Recursive topology)
└── AssetRegistry (Logical VFS)
```

### Key Components

- **Interpreter**: The Lua runtime with a dual-tier library: standard names and "performer shorthands" (mini-notation).
- **Registry**: Central node factory using unified lowercase keys and C++ class mapping.
- **Shadow Processors**: Internal high-performance workers using wait-free SPSC queues to guarantee glitch-free performance.

## Data Types & Flows

Crumble uses a two-phase **Push-Pull** model to ensure sample-accurate sync between patterns, audio, and GPU video.

### 1. The Push Phase (Sync & Control)
The Session pushes a **Timing Context** to all nodes at the start of every hardware block.
- **Type (`Control`)**: A vectorized block of `float` values (**K-rate**).
- **The Flow**: Nodes pre-calculate `Patterns` into high-speed buffers, enabling **Sample-Accurate Modulation**.

### 2. The Pull Phase (Signal Processing)
Data is pulled through the graph only when an output device demands it.
- **Audio (`ofSoundBuffer`)**: Multi-channel floating-point PCM pulled by `AudioOutput`.
- **Video (`ofTexture*`)**: GPU-resident textures pulled by `VideoOutput`. This "Zero-Copy" flow enables high-performance mixing.

## Media Management

Crumble features a **Logical Media Engine** that decouples scripts from physical file locations.

### Unified Asset Loading
- **Bank Index**: `s1.path = "drums:5"`
- **Logical Name**: `s1.path = "birds"`

## Lua DSL

### Concise Construction
Crumble supports both descriptive factory functions and performer shorthands.

```lua
-- Standard Tier
local smp = sampler("kick", { path = "drums:0" })

-- Performer Tier (Auto-names to "s1", "s2", etc.)
local s1 = s({ path = "perc:2", gain = 0.8 })
local a1 = amix() 

-- Method Chaining
s1:gain(osc(0.5)):connect(a1)
```

### Routing
```lua
-- connect() returns the assigned input slot index
local layer = s1:connect(amix)
amix["gain_" .. layer] = 0.5
```

### Modulation
```lua
-- Sample-accurate math engine
s1.speed = seq("1 2 4") * osc(0.5)
s1.gain = ramp(1):scale(0, 1):snap(4)
```

#### Pattern Library

| Function | Description |
|----------|-------------|
| `osc(f)` | Sine wave (cycles-per-bar) |
| `ramp(f)` | Sawtooth (0.0 to 1.0) |
| `noise(f)`| Smooth dynamic modulation |
| `seq("...")`| Discrete step sequencer |

| Method | Description |
|--------|-------------|
| `.fast(n)`| Speed up pattern |
| `.shift(o)`| Offset phase |
| `.scale(l, h)`| Map range |
| `.snap(s)`| Quantize steps |

## Node Reference

| Category | Type | Alias | Description |
|----------|------|-------|-------------|
| **Core** | `Graph` | `sub` | Nested scriptable sub-graph |
| **Video** | `VideoSource` | `v` | HAP video player |
| | `VideoMixer` | `vmix` | GPU compositor |
| | `VideoOutput` | `vout` | Master video sink |
| **Audio** | `AudioSource` | `s` | RAM-cached sample player |
| | `AudioMixer` | `amix` | Multi-channel summation |
| | `AudioOutput` | `aout` | Master audio sink |
| **AV** | `AVSampler` | `av` | Unified A/V playback |

## Robustness
- **Null-Safety**: Setting parameters to `nil` is safe.
- **State Preservation**: The C++ engine maintains the last valid graph state during Lua errors.

## Shortcuts
| Key | Action |
|-----|--------|
| `G` | Toggle Graph UI |
| `Cmd+S` | Save state to `main.json` |

## Shortcuts

| Key | Action |
|-----|--------|
| `G` | Toggle Graph UI |
| `Cmd+S` | Save current graph state to `main.json` |
