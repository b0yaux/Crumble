# Crumble

Audio+video live-coding node-graph built with openFrameworks and Lua.

Write Lua scripts that define a directed graph of processing nodes — samplers, mixers, delays, outputs — with sample-accurate modulation from mathematical patterns, MIDI, OSC, and gamepad input. Audio runs on a real-time thread, video on the GPU. Lua executes on the main thread, hot-reloaded on every file save.

## Installation

```bash
brew install ffmpeg sdl2
```

Clone into your openFrameworks `apps/myApps` folder:
```bash
cd $OF_ROOT/apps/myApps
git clone https://github.com/b0yaux/Crumble.git
```

Install the ofxHapPlayer addon:
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

Run one Crumble instance per script from a single command:

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

- **Node**: A processing unit — audio player, video player, mixer, delay, output.
- **Graph**: A recursive container of nodes. Graphs are themselves nodes, enabling nested sub-graphs.
- **Session**: Root container managing the graph, hardware I/O, and threading.

Crumble decouples Lua script execution from real-time audio and video processing using a **shadow processor** architecture. Lua sets parameters and assigns patterns on the main thread; changes are forwarded to the audio and GPU threads via wait-free queues. This enables **sample-accurate modulation** — pattern values can update every audio sample, not just every video frame.

## Media Management

Crumble uses a **logical media engine** that decouples scripts from physical file paths. Configure media libraries in `config.json` via `searchPaths`.

### Asset Loading

Load media into nodes using logical strings:

- **Bank index**: `node.path = "drums:5"` — 6th asset in the `drums` folder
- **Logical name**: `node.path = "birds"` — finds files named `birds` across search paths
- **Direct path**: `node.path = "clips/loop.mov"` — standard file path resolution

### Data-Driven Scripts

Query the `AssetRegistry` directly to build generative graphs from folder contents:

```lua
local assets = getBank("my-folder")
for i, asset in ipairs(assets) do
    local s = sampler(asset.name)
    s.path = asset.path
end
```

## Lua API

### Graph Construction & Routing

Two wiring methods with different return values:

| Method | Returns | Use when… |
|--------|---------|----------|
| `:connect(dst)` | **self** | You want to keep configuring the source |
| `:into(dst)` | **dst** | You want to follow the signal forward |

```lua
-- :connect() returns the source — chain parameter setters after wiring
local s1 = sampler("drums:0"):on():opacity(1.0):blend("ADD"):connect({vmix, amix})
s1:speed(1.5)                        -- still configuring s1

-- :into() returns the destination — build serial chains
drum:into(delay({time=0.3})):connect(amix)

-- mix() sets both audio gain and video opacity at once
s1:mix(0.5)
s1:mix(seq("1 0.5"):fast(4))

-- Connect with mixer-side overrides
s1:connect(vmix, {blend="ADD", opacity=0.5})
```

Both methods accept an array of destinations for parallel routing:

```lua
s1:connect({vmix, amix})             -- fan-out, returns s1
drum:into(delay({time=0.3})):connect({amix, vmix})
```

### Sequencing & Modulation

Crumble has a sample-accurate pattern engine. Most patterns are **stateless** — pure functions of the musical cycle (`cycle → float`). A few are **stateful** — they maintain internal state for accumulation, smoothing, or toggling. Both compose through the same chainable API.

```lua
local smp = sampler("bd")

-- Stateless: sequence modulated by an LFO
smp.speed = seq("1 2 4") * osc(0.5)

-- Stateful: gamepad stick accumulates position over time
smp.position = gpad("rx"):accum(0.5)
```

#### Pattern Library

**Generators:**

| Function | Description |
|----------|-------------|
| `osc(f)` / `sine(f)` | Sine wave (frequency in cycles-per-bar) |
| `ramp(f)` / `saw(f)` | Sawtooth, 0.0 → 1.0 (frequency in cycles-per-bar) |
| `noise(f, s)` | Deterministic noise (frequency `f`, optional seed `s`) |
| `seq("...")` | Discrete step sequencer |

**Stateless transforms (chain on any pattern):**

| Method | Description |
|--------|-------------|
| `p:fast(n)` | Speed up by factor `n` |
| `p:slow(n)` | Slow down by factor `n` |
| `p:shift(o)` | Phase offset (0.0 to 1.0) |
| `p:scale(lo, hi)` | Map output range to [lo, hi] |
| `p:snap(s)` | Quantize into `s` steps |
| `p:abs()` | Absolute value |
| `p:pow(e)` | Power curve. Sign-preserving: `e < 1` = more detail near zero, `e > 1` = more detail near extremes |
| `p1 * p2` | Multiply (amplitude modulation) |
| `p1 + p2` | Add (offset) |

**Stateful transforms:**

| Method | Description |
|--------|-------------|
| `p:accum(rate, initial)` | Integrate over time at `rate` per second. Clamps to [0, 1]. Negative rate goes backward. `initial` sets starting value (default 0). |
| `p:smooth(tau)` | Exponential slew limiter (one-pole low-pass). `tau` in bars — smaller = faster response. |
| `p:toggle(thresh)` | Flip 0/1 on rising edge crossing `thresh` (default 0.5). |

### Sample Sequencing (Mini-Notation)

Tidal/Strudel-style mini-notation for sample triggering via `:path(seq(...))`:

```lua
-- Define short aliases for samples
alias("k", "drums:0")
aliases({ s = "drums:1", t = "travaux" })

local s = sampler("drums")
s:path(seq("k ~ s ~ t"))             -- kick, rest, snare, rest, travaux
```

**Mini-notation syntax:**

| Symbol | Meaning |
|--------|---------|
| `~` | Rest (silence) |
| `[0 1]` | Subdivision (play both in one step's time) |
| `0*3` | Repetition (play three times) |
| `drums:0` | Bank:index notation |
| `bd sd hh` | Named samples (resolved via aliases) |

### FFT Spectral Analysis

Enable spectral analysis on any node with an audio processor. The analysis runs on the audio thread and exposes frequency data as patterns that compose like any other.

```lua
drum:fft(2048)                       -- enable FFT with 2048-bin resolution
drum:connect(amix)

-- Use spectral data as modulation sources
someNode.opacity = drum:bass():scale(0, 2)
someNode.gain = drum:mid():smooth(0.9)
```

| Method | Description |
|--------|-------------|
| `node:fft(size)` | Enable FFT analysis (power-of-2 size) |
| `node:bin(i)` | Single bin value |
| `node:bins(lo, hi)` | Average of bin range |
| `node:band(loHz, hiHz)` | Average of frequency band |
| `node:bass()` | 20–250 Hz |
| `node:lowmid()` | 250–500 Hz |
| `node:mid()` | 500–2000 Hz |
| `node:high()` | 2000–20000 Hz |
| `node:rms()` | Overall RMS level |

### Video Blending

`VideoMixer` composites layers using a single-pass GPU shader. Each connected node carries its own `blend` and `opacity` parameters:

```lua
s1:blend("ADD"):opacity(0.8):connect(vmix)
```

Four blend modes, accessible via the `BLEND` table or string names:

| Mode | Value | Description |
|------|-------|-------------|
| `ALPHA` | 0 | Standard alpha blending |
| `ADD` | 1 | Additive — layers brighten the output |
| `MULTIPLY` | 2 | Multiplicative — layers darken/tint |
| `SCREEN` | 3 | Screen — brightens without over-saturating |

### Tempo

| Function | Description |
|----------|-------------|
| `bpm(v)` | Set tempo in beats per minute |
| `cpm(v)` | Set tempo in cycles per minute (`bpm × 4` in 4/4) |
| `cps(v)` | Set tempo in cycles per second |

All pattern frequencies are in **cycles per bar**. `Transport.cycle` advances at `bpm / beatsPerBar` beats per second, wrapping once per bar. Default is `beatsPerBar = 4` (common time).

| `beatsPerBar` | Time sig | Bar length at 120 BPM | `osc(1.0)` rate |
|---|---|---|---|
| 4 (default) | 4/4 | 2.0 s | 0.5 Hz |
| 3 | 3/4 | 1.5 s | 0.67 Hz |
| 5 | 5/4 | 2.5 s | 0.4 Hz |

To modulate at beat rate in 4/4, use `osc(4.0)` (or equivalently `osc(1.0):fast(4)`).

### Per-Frame Callback

Define an `update()` function to run logic every frame:

```lua
function update()
    local t = Time.absoluteTime       -- seconds since start
    local cycle = Time.cycle           -- current cycle position
    local bars = Time.bars             -- bar count
    local bpm = Time.tempo             -- current BPM
end
```

### Global Constants

| Name | Description |
|------|-------------|
| `BLEND` | `{ALPHA=0, ADD=1, MULTIPLY=2, SCREEN=3}` — blend mode constants |
| `GPAD` | Gamepad button/axis name mappings |

### Node Aliases

| Alias | Equivalent |
|-------|-----------|
| `s(name)` | `sampler(name)` |
| `amix()` | `audiomix()` |
| `vmix()` | `videomix()` |
| `aout()` | `audioout()` |
| `vout()` | `videoout()` |

### Sub-Graph Composition

Graphs are recursive — a `graph` node loads a nested script in an isolated environment:

```lua
local g = graph("mySubgraph", { script = "scripts/inner.lua" })
```

Sub-graphs use **boundary nodes** to connect to their parent:

```lua
-- scripts/inner.lua
local inlet = addNode("inlet", "in")
local proc = addNode("audiomix", "mix")
local outlet = addNode("outlet", "out")
connect(inlet, proc)
connect(proc, outlet)
```

In the parent, connect to the sub-graph as if it were any other node:

```lua
local src = addNode("audio", "src")
local g = graph("sub", { script = "scripts/inner.lua" })
connect(src, g)                       -- routes through inlet/outlet boundaries
```

The `sampler()` node is itself a sub-graph — it loads `scripts/nodes/avsampler.lua` to coordinate an `audio` and a `video` child with synchronized playback. The `expose()` function forwards parent parameters to children.

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

> All required modules share the same global namespace. Node names must be unique across all loaded modules.

### Live Reload

| Trigger | Behavior |
|---------|----------|
| `.lua` file saved | Hot-reload: existing nodes keep state, new nodes created, removed ones deleted |
| `entryScript` changed in `config.json` | Full reset: graph cleared, new script starts fresh |
| `config.json` saved | Reload configuration |

Editing the current script preserves playback state. Switching to a different script provides a clean slate.

## Node Reference

| Category | Type | Factory | Description |
|----------|------|---------|-------------|
| **Core** | `Graph` | `graph()` | Nested scriptable sub-graph |
| | `Inlet` | `addNode("inlet")` | Sub-graph input boundary |
| | `Outlet` | `addNode("outlet")` | Sub-graph output boundary |
| **Video** | `VideoSource` | `video()` | HAP video player |
| | `VideoMixer` | `videomix()` / `vmix()` | Multi-layer GPU compositor |
| | `VideoOutput` | `videoout()` / `vout()` | Master video sink |
| **Audio** | `AudioSource` | `audio()` | RAM-cached sample player |
| | `AudioMixer` | `audiomix()` / `amix()` | Multi-channel summation |
| | `Delay` | `delay()` | Feedback delay line |
| | `AudioOutput` | `audioout()` / `aout()` | Master audio sink |
| **AV** | — | `sampler()` / `s()` | Unified audio+video player (Lua sub-graph) |

## Hardware Input

Crumble supports real-time modulation from MIDI, OSC, and gamepad controllers. All input values are normalized to 0.0–1.0.

### MIDI

```lua
-- Control Change (knobs, faders)
s1:gain(midi(74, 1))                 -- CC 74 on channel 1

-- Note velocity (pad strike intensity)
s1:opacity(midinote(36, 10))         -- note 36 on channel 10

-- Polyphonic aftertouch (per-key pressure)
s1:speed(miditouch(36, 10):scale(0.5, 2.0))

-- Channel aftertouch (keyboard pressure)
s1:speed(channeltouch(1):scale(0.5, 2.0))
```

### MIDI Events

Drain discrete note events in `update()` for trigger logic:

```lua
function update()
    for _, e in ipairs(midievents(1)) do   -- channel 1 (nil = all channels)
        if e.on then
            print("note on:", e.note, e.velocity)
        end
    end
end
```

Each event is a table: `{ on, note, velocity, channel, time }`.

### OSC

```lua
-- Listens on port 8000 (default)
s1:gain(oscin("/filterCutoff"))
```

### Gamepad

```lua
-- Auto-detects button or axis. Both Xbox and PlayStation naming supported.
s1:gain(gpad("cross"))                -- A/Cross button
s1:mute(gpad("l1"))                   -- LB/L1 bumper
s1:speed(gpad("ly"):scale(0.5, 2.0))  -- left stick Y
s1:opacity(gpad("rx"):scale(0, 1))    -- right stick X

-- Available names:
-- Face:    a/cross, b/circle, x/square, y/triangle
-- Shoulders: lb/l1, rb/r1
-- Menu:    back/select, start/options, guide/ps
-- Sticks:  ls/l3, rs/r3 (press), lx/ly/rx/ry (axes), lt/rt (triggers)
-- D-pad:   up, down, left, right
```

### Pattern Composition

All hardware inputs return patterns that compose with the full chain API:

```lua
-- Combine MIDI CC and note velocity
s1:gain(midi(82, 1) + midinote(36, 10))

-- Scale and transform gamepad input
s1:speed(gpad("rx"):scale(-2, 2))

-- Gamepad with accumulation and smoothing
s1:gain(gpad("ry"):accum(0.3):smooth(2.0))
```

### Event Primitives in `update()`

Two layers for responding to input:

1. **Pattern modulation** — continuous signal flow into parameters, evaluated per-sample or per-frame. This is what all the examples above use.

2. **Lua event primitives** — discrete decisions in `update()`, used when input should trigger logic (randomize, create/destroy nodes, step counters) rather than modulate a parameter.

Three event primitives accept any input source (string, number, or gen table):

```lua
function update()
    -- once(source): fires once per press, no repeat
    if once("cross") then idx = math.random(0, total - 1) end
    if once(gpad("triangle")) then blend = blend % #blends + 1 end

    -- press(source, delay, rate): fires immediately, then auto-repeats while held
    if press("r1") then batch = batch + 1 end
    if press("l1") then batch = batch - 1 end

    -- held(source): true every frame while held
    if held("up") then opacity = math.min(1, opacity + 0.02) end
    if held("down") then opacity = math.max(0, opacity - 0.02) end
end
```

| Primitive | Fires | Use for |
|-----------|-------|---------|
| `once(source)` | Once per press | Randomize, cycle mode, toggle |
| `press(source, delay, rate)` | Once + auto-repeat | Step values, scroll lists |
| `held(source)` | Every frame while held | Analog-style continuous adjustment |

## Robustness

- **Null-safety**: Setting parameters to `nil` or passing `nil` to routing functions logs a warning without crashing.
- **State preservation**: The C++ engine maintains the last valid graph state when a Lua script encounters runtime errors.

## Shortcuts

| Key | Action |
|-----|--------|
| `G` | Toggle graph UI |
| `Cmd+S` | Save graph state to `main.json` |
| `Cmd+R` | Reload current script |
