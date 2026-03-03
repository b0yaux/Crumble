# Crumble Architecture

Crumble is a minimal, modular video and audio graph system. The architecture is built around a pull-based data flow combined with a pushed timing context, allowing sample-accurate functional sequencing without tightly coupling timing logic to the individual DSP/video nodes.

## Core Paradigms

1. **Pull-Based Data Flow**: 
   - Audio is actively pulled by the audio hardware callback (sink-driven). The master `SpeakersOutput` requests a buffer fill from its input, which recursively requests fills up the graph.
   - Video is passively pulled during the UI render loop (`draw()`).

2. **Pushed Timing Context (Sample-Accurate Modulation)**:
   - Before audio is pulled, the `Session` pushes a `Context` packet containing the absolute musical cycle and cycle step size to all nodes in the graph.
   - Base `Node` instances use this context to pre-calculate vectorized `Control` buffers for any modulated parameters via a tree of math `Pattern` objects (`Seq`, `Osc`, etc.).
   - During `pullAudio()`, nodes iterate over these pre-calculated buffers using `getControl(param)`, ensuring lock-free, sample-accurate evaluation of modulated parameters.

3. **No-Port Graph Topology**:
   - Connections are managed at the `Graph` level using integer node IDs rather than object pointers or string names.
   - Nodes do not explicitly own their inputs or outputs. Instead, the `Graph` resolves connections during the pull phase.
   - `Graph` instances are themselves `Node`s, allowing infinite hierarchical nesting (subgraphs).

4. **Script-Driven State**:
   - The primary interface is a Lua DSL.
   - Scripts are executed idempotently: nodes touched during execution are kept, and untouched nodes are garbage-collected (`beginScript()` / `endScript()`).
   - The C++ environment provides a reactive mapping (via the `Interpreter`) to translate Lua code into C++ expression trees and parameter assignments.

## Class Overview

### Session
- Manages the root `Graph`, the `Transport` clock, the `AssetCache`, and the Lua `Interpreter`.
- Owns the `ofSoundStream` master audio callback. The hardware audio callback strictly drives the global `Transport` clock.

### Graph
- A subclass of `Node` that manages a directed acyclic graph (DAG) of child nodes and their connections.
- Handles topological sorting for UI `update()` traversal.
- Intercepts audio/video pulls and routes them to special `Inlet` and `Outlet` nodes for sub-graph boundary routing.

### Node
- The base entity. Holds an `ofParameterGroup` and a map of `Pattern` modulators.
- `prepare(Context)`: Pre-calculates control signals if block size > 1.
- `getControl(param)`: Returns a vectorized `Control` struct for block iteration.
- `pullAudio(buffer, index)`: Overridden by audio-producing nodes.
- `getVideoOutput(index)`: Overridden by video-producing nodes.

### Patterns (Modulators)
- Mathematical expression tree components (`Seq`, `Osc`, `Ramp`, `Add`, `Mul`) evaluated at sub-sample precision.
- Built from Lua via operator overloading (`*`, `+`) and DSL functions (`seq("1 0 1 0")`).

### Interpreter
- Binds the C++ Session/Graph primitives to Lua.
- Maintains an RAII execution stack to support nested graph loading.

## Threading Model
- **UI Thread (~60fps)**: Handles script parsing via the `Interpreter`, Node creation/deletion (`createNode`, `removeNode`), and video processing (`update()`, `draw()`).

- **Audio Thread (Hardware Rate)**: Executes `Session::audioOut()`, calls `prepare(ctx)` on all nodes, and triggers `pullAudio()`. Protected by `Graph::audioMutex`.

## Flaws & Identified Technical Debt
During the recent refactoring, a few critical areas were identified that require attention:
1. **Missing Audio Mutex in `Graph::createNode`**: `createNode` modifies the `nodes` unordered_map on the UI thread without locking `audioMutex`, while `Session::audioOut` iterates over `nodes` on the audio thread. This can cause iterator invalidation and crashes.
2. **Thread-Safety of Playheads**: `AudioFileSource::playhead` is a standard `double` mutated in the audio thread but read in the UI thread via `getRelativePosition()` (e.g., to sync video in `AVSampler`). It should be `std::atomic<double>`.
3. **Redundant UI updates on Parameters**: `AVSampler::update()` constantly calls `position.set(audioPos)`. While harmless currently, this triggers UI listeners on every frame and could cause performance hiccups if complex listeners are attached.
