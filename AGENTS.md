# AGENTS.md — Crumble Project Context

## Project Summary

Crumble is a real-time audio+video live-coding node-graph system built with openFrameworks (C++) and Lua. Users write Lua scripts that define a directed graph of processing nodes (samplers, mixers, outputs) with sample-accurate modulation via mathematical patterns. The system uses a **Shadow Processor** architecture to decouple slow Lua/UI logic from real-time audio (RtAudio callback) and GPU video (main-thread OpenGL).

## Build & Run

```bash
make                # Debug build
make Release        # Release build
make RunRelease     # Build and run
```

Build system is openFrameworks `makefileCommon/compile.project.mk`. No cmake, no npm.

**Note for Agents:** `make RunRelease` launches a blocking GUI window. The agent cannot interact with it — the user must test visual output. To verify code changes compile, use `make`. Use `make RunRelease` only when the user can observe the result.

### Config

- `bin/data/config.json` — entry script, search paths, graph UI settings
- `bin/data/system/prelude.lua` — Lua DSL standard library (factory functions, patterns, expose(), input event primitives)
- Entry script defaults to `scripts/main.lua`

### Command-line

```bash
./Crumble                              # Default config
./Crumble -s scripts/drums.lua         # Override script
./Crumble -a scripts/                  # Multi-instance: one per .lua file
```

## Source Tree

```
src/
├── main.cpp                  — CLI parsing, multi-instance launcher
├── ofApp.h/cpp               — oF lifecycle hooks, delegates to Session
├── core/
│   ├── Session               — Root container: threading, command dispatch, hardware I/O
│   ├── Graph                 — Recursive node container, sub-graph lifecycle, topology
│   ├── Node                  — Base node: parameters, modulators, shadow processor bridge
│   ├── NodeProcessor         — AudioProcessor, VideoProcessor, ControlSlot, evalSlot()
│   ├── ProcessorCommand      — Command enum + struct for SPSC queues
│   ├── Interpreter           — Lua bindings, script execution, live-reload
│   ├── Patterns              — Stateless pattern classes (Seq, Osc, Noise, etc.)
│   ├── PatternMath           — Pattern composition (Fast, Slow, Scale, Snap, etc.)
│   ├── Transport             — Musical clock (cycle, BPM, beatsPerBar)
│   ├── AssetRegistry         — Logical VFS: banks, name resolution, A/V pairing
│   ├── AudioCache            — Deduplicated RAM audio buffers
│   ├── VideoCache            — ofxHapPlayer pooling and caching
│   ├── Config                — JSON config loader
│   ├── InputBindings         — MIDI, OSC, gamepad → atomic floats + Lua event primitives bridge
│   ├── Registry              — Node type factory (type string → constructor)
│   ├── FileWatcher           — File system watcher for live-reload
│   └── moodycamel/           — Lock-free SPSC queue (third-party)
├── nodes/
│   ├── AudioSource           — RAM-cached sample player, trigger system, loop region
│   ├── VideoSource           — HAP video player, clock modes (INTERNAL/EXTERNAL)
│   ├── AVSampler             — DEPRECATED. Lua sub-graph is canonical.
│   ├── AudioMixer            — Multi-channel summation
│   ├── VideoMixer            — Single-pass GPU compositing with custom shader, chunked accumulator for >15 layers
│   ├── AudioOutput           — Hardware audio sink
│   ├── VideoOutput           — Display sink
│   └── composite/            — (reserved for future composite nodes)
└── ui/
    └── GraphUI               — Node graph visualization with physics layout
```

### Shaders

```
bin/data/shaders/
├── composite.vert            — Passthrough vertex shader (position + texcoord)
└── composite.frag            — Single-pass blend: ALPHA, ADD, MULTIPLY, SCREEN with per-layer opacity
```

oF uses `OF_GLSL_SHADER_HEADER` as a placeholder preamble — replaced at load time with the correct `#version` directive. Vertex shader must use attribute names `position`, `texcoord` when `bindDefaults = true`. ofxHapPlayer produces `GL_TEXTURE_2D` textures — shader must use `sampler2D`, not `sampler2DRect`.

## Architecture Key Points

### Threading Model

- **Main thread**: Lua execution, video processing (GPU/GL context), UI, file watching
- **Audio thread**: RtAudio callback, sample-accurate DSP, pattern evaluation per-sample
- **Communication**: Main→Audio via wait-free SPSC queues. Audio→Main via atomic flags + garbage queues.

### Shadow Processors

Each Node can have an `AudioProcessor` and/or `VideoProcessor`. These are the "shadow" copies that run on the real-time threads. Parameters flow from Lua → `ofParameter` → `pushCommand(SET_PARAM/SET_PATTERN)` → SPSC queue → shadow processor slot.

### Node Lifecycle

1. **Creation**: Lua `addNode()` → `Graph::createNode()` → `setupProcessor()` → `ADD_NODE` command
2. **Live-reload (hot-reload)**: `beginScript()` marks nodes untouched → script re-executes, touching nodes → `endScript()` destroys untouched nodes + clears untouched modulators → batch-drain to SPSC queues
3. **Destruction**: `Node::~Node()` → `REMOVE_NODE` command → audio thread removes processor

### Sub-graph System

- Graphs are Nodes. `graph("name", {script="..."})` loads a Lua script in an isolated `_ENV`.
- `expose()` registers proxy targets so parent parameters forward to children.
- Boundary routing via `resolveAudioOutput()`/`resolveVideoOutput()` typed methods.
- `sampler()` is now a Lua sub-graph loading `scripts/nodes/avsampler.lua` (C++ AVSampler deprecated).

### Pattern System

Patterns are stateless functions `cycle → float`. Evaluated at three rates:
- **Per-sample** (audio thread): speed, gain, position, loopSize — via `ControlSlot` + `evalSlot()`
- **Per-frame** (main thread): video speed, active — via `ControlSlot`
- **Per-trigger** (event): path triggers — via `querySlot()` + atomic flags

### Input System

Two layers for responding to hardware input (gamepad, MIDI, OSC):

1. **Pattern modulation** — continuous signal flow into node parameters. Input sources (`gpad()`, `midi()`, `oscin()`) return pattern generators that compose through the chain API. Evaluated on audio/video threads via `ControlSlot`.

2. **Lua event primitives** — discrete decisions in `update()`. `once()`, `press()`, `held()` accept any source (string, number, gen table). They read input values from the `InputBindings` atomics, not from the pattern system. Used for imperative logic: randomize, step counters, create/destroy nodes.

The C++ bridge is `_readBinding()` (`Interpreter::lua_readBinding`) — takes a gen table, resolves the type/id to a binding path, reads the atomic float, returns it to Lua. This makes `gpad("cross")` usable both as a pattern source and as a readable value in Lua event logic.

## Code Conventions

- **C++ style**: openFrameworks idioms. `ofParameter<float>` for exposed params, `shared_ptr` for ownership.
- **Headers**: Forward-declare types in headers; include full headers in .cpp files.
- **Node parameters**: Declared as `ofParameter<T>` members added to `parameters` (ofParameterGroup). Accessed by name string for shadow sync.
- **Commands**: All shadow processor mutations go through `pushCommand()`. Never touch a processor directly from the main thread if it's also accessed from the audio thread.
- **Documentation**: Headers document the public API — class purpose, design intent, method contracts. .cpp files document non-obvious logic, algorithms, and rationale. Self-evident code stays clean. Ground terms when they first appear (not everyone knows what "shadow processor" or "SPSC queue" means).
- **Lua DSL**: Defined in `bin/data/system/prelude.lua`. Node factories return node objects with chainable methods.
- **Naming**: Node types use PascalCase (e.g., `AudioSource`), Lua aliases use lowercase (e.g., `audio()`).

## Known Issues & Technical Debt

### Active Issues (Obsidian)

Located at `/Users/jaufre/works/notes/Programming/Active projects/Crumble/Issues/`:

| Issue | Status | Priority |
|-------|--------|----------|
| Main-Thread Audio Decoding Hitch | PENDING | High (UX) — 50-200ms freeze on first asset load |
| ofxHapPlayer Initialization Overhead | PENDING | Medium — stutter on rapid asset swaps |

### Potential Issues (unverified)

From `Issues/? — sloppy issues (to verify).md` — self-described as potentially outdated but contains plausible leads:

- `ProcessorCommand` holds `shared_ptr<Pattern>` → destructor may run on audio thread (allocation-free violation)
- `FileWatcher` holds mutex during `sleep()` → 500ms reload latency
- `AudioMixer` silently drops inputs above index 15 (hardcoded loop bound)
- SPSC queue overflow is silent data loss (dropped `RELEASE_BUFFER` → use-after-free window)

### Deprecated Code

- `src/nodes/AVSampler.cpp/h` — C++ AVSampler is deprecated. Lua sub-graph is canonical. Retained as reference only.

## Documentation (Obsidian Vault)

Primary project documentation lives in an Obsidian vault at:
```
/Users/jaufre/works/notes/Programming/Active projects/Crumble/
```

| File | Purpose |
|------|---------|
| `Architecture.md` | ADRs, threading model, parameter system |
| `Health.md` | Codebase health metrics (line counts, file distribution) |
| `Crumble.md` | User-facing project overview |
| `Roadmaps/Roadmaps.md` | Future planning notes |
| `Roadmaps/31.03.26 AVSampler Roadmap.md` | AVSampler sub-graph implementation (complete) |
| `Issues/` | Bug tracking with FIXED/PENDING status |

### Obsidian CLI

Use the `bash` tool to run the `obsidian` command-line utility, which provides direct access to the vault. Useful commands:

```bash
obsidian read file="Architecture"                          # Read a note by name
obsidian search query="ADR-009" path="Active projects/Crumble"  # Search the vault
obsidian outline file="Architecture"                       # Show headings
obsidian backlinks file="Architecture"                     # Show backlinks
```

**Note:** For writing to Obsidian files, prefer direct file editing via the Write/Edit tools rather than `obsidian append`/`obsidian create`. The CLI requires escaping special characters (`\n`, `\"`, backticks) which is fragile for content with code blocks or complex markdown.

**Waypoints:** Obsidian auto-updates `%% Begin Waypoint %%` / `%% End Waypoint %%` blocks when notes are moved or renamed. These cross-references are maintained automatically — no manual sync needed.

The vault path is:
```
/Users/jaufre/works/notes/Programming/Active projects/Crumble/
```

### Updating Obsidian Docs

When making architectural changes:
1. Add new ADRs to `Architecture.md` (Context/Decision/Consequence format)
2. Create issue files in `Issues/` for new bugs (Status: PENDING/FIXED, Priority, Related files)
3. Update `Health.md` line counts after significant refactors
4. When `Crumble.md` and `README.md` diverge, prioritize README.md as the source of truth

### External Research

When researching new primitives, DSP techniques, or routing patterns, search the Obsidian vault at `/Users/jaufre/works/notes/Programming/` for relevant reference notes (e.g., JUCE, Bespoke Synth, TidalCycles, Hydra, openFrameworks addons). Use `webfetch` on URLs found in those notes. Compare findings with Crumble's Research documents before proposing changes.

## File Watching & Live-Reload

- `.lua` files: Hot-reload via `FileWatcher` (polls every 500ms). Existing nodes keep state, new nodes created, removed nodes deleted.
- `config.json`: Reloads configuration.
- `entryScript` change: Full graph reset.
