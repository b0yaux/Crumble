# Crumble

Simple audio+video live-scriptable node-graph system built with openFrameworks and Lua.

## Installation

```bash
brew install ffmpeg sdl2
```

Clone into your openFrameworks `apps/myApps` folder:
```bash
cd $OF_ROOT/apps/myApps
git clone https://github.com/b0yaux/Crumble.git
```

Install the ofxHapPlayer addon (uses system ffmpeg from Homebrew):
```bash
cd $OF_ROOT/addons
git clone https://github.com/b0yaux/ofxHapPlayer.git
```

Build and run:
```bash
cd Crumble
make Release
make RunRelease
```

> `$OF_ROOT` is your openFrameworks installation path (e.g. `~/works/of_v0.12.1_osx_release`).

## Quick Start

```bash
cd Crumble
make
make RunRelease     # loads bin/data/config.json
```

### Command-Line Options

```bash
./Crumble                              # Default: config.json → entryScript
./Crumble -s scripts/drums.lua         # Override script (relative to bin/data/)
./Crumble -c drums.json                # Use different config file
./Crumble -t "Drums" -s drums.lua      # Set window title + script
```

| Flag | Description | Default |
|------|-------------|---------|
| `-c, --config` | Config file path | `config.json` |
| `-s, --script` | Override entry script (relative to `bin/data/` or absolute) | (from config) |
| `-t, --title` | Window title | (none) |
| `-a, --run-all` | Launch one instance per `.lua` file in a directory | (none) |

### Multi-Instance

Run one Crumble instance per script — all from a single command:

```bash
./Crumble -a scripts/
# Launches: main.lua, stratum.lua, Nous.lua, ...
# Ctrl+C kills all instances at once
```

Or run individual scripts in separate terminals:

```bash
./Crumble -s scripts/drums.lua -t "Drums"
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
Crumble supports **Auto-Indexing**, **Table Routing**, and **Chainable Parameters** for concise graph setup.
```lua
-- Create and configure in one line
local s1 = sampler("drums:0"):on():opacity(1.0):blend("ADD"):connect({vmix, amix})

-- Chainable methods return the node, so you can keep configuring:
s1:speed(1.5):connect(vmix)

-- mix() controls both audio gain and video opacity simultaneously:
s1:mix(0.5)                           -- constant value
s1:mix(seq("1 0.5"):fast(4))         -- pattern-based modulation

-- Advanced connect with mixer-side overrides:
s1:connect(vmix, {blend="ADD", opacity=0.5})
```

### Sequencing & Modulation

Crumble features a sample-accurate pattern engine. Most patterns are **stateless** — pure functions of the musical cycle (`cycle → float`). A few are **stateful** — they remember previous output for accumulation, smoothing, or toggling. Both types compose through the same chainable API.

```lua
local smp = sampler("bd")

-- Stateless composition: sequence modulated by an LFO
smp.speed = seq("1 2 4") * osc(0.5)

-- Stateful: gamepad stick accumulates position over time
smp.position = gpad("rx"):accum(0.5)
```

#### Pattern Library

**Generators:**

| Function | Description |
|----------|-------------|
| `osc(f)` / `sine(f)` | Sine wave (frequency `f` in cycles-per-bar) |
| `ramp(f)` / `saw(f)` | Sawtooth (0.0 to 1.0, frequency `f` in cycles-per-bar) |
| `noise(f, s)` | Deterministic stochastic noise (frequency `f`, optional seed `s`) |
| `seq("...")` | Discrete step sequencer |
| `sp("...")` | **Strudel-style sampler pattern** using aliases |

**Transforms (method syntax — chain on any pattern):**

| Method | Description |
|--------|-------------|
| `p:fast(n)` | Speed up by factor `n` (constant or pattern) |
| `p:slow(n)` | Slow down by factor `n` (constant or pattern) |
| `p:shift(o)` | Offset phase by `o` (0.0 to 1.0) |
| `p:scale(l, h)` | Map output range to [low, high] |
| `p:snap(s)` | Quantize output into `s` steps |
| `p:abs()` | Absolute value |
| `p:pow(e)` | Power curve (`e<1` = more detail near zero, `e>1` = more detail near extremes). Sign-preserving. |
| `p1 * p2` | Multiply two patterns (Amplitude Modulation) |
| `p1 + p2` | Add two patterns (Offset/Mixing) |

**Stateful Transforms (maintain internal state across evaluations):**

| Method | Description |
|--------|-------------|
| `p:accum(rate, initial)` | Integrate `p` over time at `rate` per second. Clamps to [0, 1]. `initial` sets starting value. Use negative rate to go backward. |
| `p:smooth(tau)` | Exponential slew limiter (one-pole low-pass). `tau` in bars. Smaller = faster response. |
| `p:toggle(thresh)` | Flip boolean on rising edge crossing `thresh` (default 0.5). |

```lua
-- Accumulator: stick navigates a value (centered = hold position)
local pos = gpad("rx"):accum(0.5)                -- 0.5/sec, starts at 0
local spd = 1.0 + gpad("ly"):accum(-0.5, 0.5):scale(-3, 3)  -- starts at 0.5 → speed 1.0

-- Smoothing: remove jitter from noisy inputs
local gain = gpad("rx"):accum(0.3):smooth(2.0)

-- Power curve: fine control at small values
local lsz = gpad("ry"):accum(-0.3, 0.707):pow(2.0):scale(0.0001, 1)
```

### Sample Sequencing (Mini-Notation)

Crumble supports Tidal/Strudel-style mini-notation for sample triggering and **Global Aliases**:

```lua
-- Define aliases (Strudel-style)
alias("k", "drums:0")
aliases({ s = "drums:1", t = "travaux" })

local s = sampler("drums")

-- Basic patterns using aliases
s:n("k ~ s ~ t")           -- Trigger kick, rest, snare, rest, travaux
```

**Video Blending & Performance:**
- **Video Cache**: Automatic global caching ensures that switching between samples (e.g. in `sp()`) is instant and glitch-free after the first load.
- **GPU Compositing**: `VideoMixer` uses a single-pass fragment shader with four blend modes:
  - `ALPHA` (0): standard alpha blending — each layer replaces a fraction of the accumulator
  - `ADD` (1): additive — every layer brightens, good for dense multi-layer mixing
  - `MULTIPLY` (2): brightening tint — scales accumulator by layer color
  - `SCREEN` (3): commutative brightening — never over-saturates, good for light-on-dark
- **Intelligent Blending**: Nodes have their own `blend` and `opacity` parameters. `VideoMixer` automatically reads these, allowing for `s:blend("ADD")` syntax.
- **Embedded Audio**: Supports `.mov` files with embedded audio (HAP codec). The system automatically switches to embedded mode when no separate audio file is found.


**Mini-Notation Syntax:**

| Symbol | Meaning |
|--------|---------|
| `~` | Rest (silence) |
| `[0 1]` | Subdivision (play 0 and 1 in one step's time) |
| `0*3` | Repetition (play 0 three times) |
| `drums:0` | Bank:index notation |
| `bd sd hh` | Named samples (bd→0, sd→1, hh→2, etc.) |

> **Timing contract:** Stateless patterns are pure functions of the cycle. Multiple nodes can share the same pattern object without interference. Stateful patterns (`accum`, `smooth`, `toggle`) share state when shared between nodes — e.g. `s.position = pos` on 15 samplers gives them one shared accumulator, not 15 independent ones.

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

### Tempo

| Function | Description |
|----------|-------------|
| `bpm(v)` | Set tempo in beats per minute |
| `cpm(v)` | Set tempo in cycles per minute (`bpm * 4`) |
| `cps(v)` | Set tempo in cycles per second (`bpm * 240`) |

### Per-Frame Callback

Define an `update()` function to run logic every frame. The global `Time` table is available:

```lua
function update()
    local t = Time.absoluteTime   -- Seconds since start
    local cycle = Time.cycle       -- Current cycle position
    local bars = Time.bars         -- Bar count
    local bpm = Time.tempo         -- Current BPM
end
```

### Global Constants

| Name | Description |
|------|-------------|
| `BLEND` | `{ALPHA=0, ADD=1, MULTIPLY=2, SCREEN=3}` — used for video blend parameters |

### Node Short Aliases

| Alias | Equivalent |
|-------|-----------|
| `s(name)` | `sampler(name)` |
| `amix()` | `audiomix()` |
| `vmix()` | `videomix()` |
| `aout()` | `audioout()` |
| `vout()` | `videoout()` |

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

-- Note-specific aftertouch (key pressure)
s1:speed(miditouch(36, 10):scale(0.5, 2.0))

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
-- Unified API - auto-detects button or axis
-- Both Xbox and PlayStation naming supported
s1:gain(gpad("cross"))                  -- A/Cross button
s1:mute(gpad("l1"))                     -- LB/L1 bumper
s1:speed(gpad("ly"):scale(0.5, 2.0))    -- Left stick Y
s1:cutoff(gpad("rx"):scale(-1, 1))     -- Right stick X

-- Available names:
-- Face: a/cross, b/circle, x/square, y/triangle
-- Shoulders: lb/l1, rb/r1
-- Menu: back/select, start/options, guide/ps
-- Sticks: ls/l3, rs/r3
-- D-pad: up, down, left, right
-- Axes: lx, ly, rx, ry, lt, rt
```

### Pattern Composition

All hardware inputs return patterns that can be composed:

```lua
-- Combine MIDI CC and note velocity
local mix = midi(82, 1) + midinote(36, 10)
s1:gain(mix)

-- Scale and transform
s1:speed(gpad("rx"):scale(-2, 2))  -- Right stick X mapped to speed
```

### Button Input in `update()`

Crumble provides two layers for responding to input:

1. **Pattern modulation** — for continuous signal flow into node parameters.
   Composed via the pattern chain: `s.speed = gpad("ly"):accum(-0.5, 0.5):scale(-3, 3)`
   Evaluated per-sample on the audio thread or per-frame on the video thread.

2. **Lua event primitives** — for discrete decisions in `update()`.
   Used when input should trigger logic (randomize, create/destroy nodes, branch, step counters) rather than modulate a parameter continuously.

Three event primitives are available: `once()`, `press()`, `held()`. All accept any input source:

| Source type | Example | What it does |
|------------|---------|-------------|
| String (gamepad name) | `"cross"`, `"l1"`, `"up"` | Auto-reads from Gamepad table |
| Number (raw float) | `g.cross`, `0.5` | Direct value |
| Gen table | `gpad("cross")`, `midi(64, 1)` | Reads the input binding via C |
| nil | `nil` | Treated as 0 |

```lua
function update()
    -- once(source): fires once per press, no repeat
    if once("cross") then idx = math.random(0, total - 1) end
    if once(gpad("triangle")) then blend = blend % #blends + 1 end

    -- press(source, delay, rate): fires immediately, then auto-repeats while held
    if press("r1") then batch = batch + 1 end
    if press("l1") then batch = batch - 1 end
    if press(gpad("lt")) then idx = (idx - 1) % total end

    -- held(source): true while button is held (continuous)
    if held("up") then opacity = math.min(1, opacity + 0.02) end
    if held("down") then opacity = math.max(0, opacity - 0.02) end
end
```

| Primitive | Fires | Use for |
|-----------|-------|---------|
| `once(source)` | Once per press | Randomize, cycle mode, toggle |
| `press(source, delay, rate)` | Once + auto-repeat | Step values, scroll lists |
| `held(source)` | Every frame while held | Analog-style continuous adjustment |
