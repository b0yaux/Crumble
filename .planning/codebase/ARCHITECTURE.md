# Architecture

**Analysis Date:** 2026-02-21

## Pattern Overview

**Overall:** Node-based Data Flow Graph with Pull-based Evaluation.

**Key Characteristics:**
- **Hierarchical Graph:** `Graph` inherits from `Node`, allowing graphs to contain other graphs (nested components).
- **Pull-based Evaluation:** Nodes pull data from upstream dependencies during `update()` or `audioOut()`, typically using `getVideoOutput()` or similar methods.
- **Stable ID System:** Nodes are referenced by persistent integer IDs (`nodeId`) rather than pointers in connections and external APIs (Lua/JSON).

## Layers

**Application Layer:**
- Purpose: Entry point and OS integration via openFrameworks.
- Location: `src/main.cpp`, `src/ofApp.cpp`
- Contains: Main loop, window management, event routing.
- Depends on: `Session`, `ScriptBridge`, `GraphUI`.
- Used by: openFrameworks runtime.

**Session Layer:**
- Purpose: Manage the active working context, assets, and undo/redo history.
- Location: `src/core/Session.cpp`
- Contains: `Session` class, `AssetPool`, snapshot-based undo history.
- Depends on: `Graph`, `AssetPool`.
- Used by: `ofApp`, `ScriptBridge`.

**Graph Layer:**
- Purpose: Topology management and execution orchestration.
- Location: `src/core/Graph.cpp`
- Contains: `Graph` class, `Connection` structures.
- Depends on: `Node`.
- Used by: `Session`.

**Node Layer:**
- Purpose: Base processing unit and concrete implementations.
- Location: `src/core/Node.cpp`, `src/nodes/`
- Contains: `Node` base class, specialized nodes (e.g., `VideoMixer`, `AudioFileSource`).
- Depends on: openFrameworks (`ofMain.h`).
- Used by: `Graph`.

**UI Layer:**
- Purpose: Visual representation and interactive manipulation of the graph.
- Location: `src/ui/GraphUI.cpp`
- Contains: `GraphUI` class.
- Depends on: `Session`, `Node`.
- Used by: `ofApp`.

## Data Flow

**Video Processing Flow:**

1. `ofApp::draw()` triggers `GraphUI::draw()`.
2. Terminal nodes (e.g., `ScreenOutput`) are queried for their output.
3. Pull request: `ScreenOutput` calls `getVideoOutput()` on its connected input node.
4. Recursive evaluation: Nodes pull from their inputs until a source node is reached.
5. Cached results: Nodes use `lastUpdateFrame` to avoid redundant calculations within a single frame.

**Audio Processing Flow:**

1. openFrameworks audio thread calls `ofApp::audioOut()`.
2. `ofApp` forwards call to `Session::getGraph().audioOut()`.
3. Graph iterates through nodes (or follows specific output nodes).
4. Nodes modify the `ofSoundBuffer` in-place.

## Key Abstractions

**Node:**
- Purpose: Atomic unit of computation or IO.
- Examples: `src/nodes/video/VideoFileSource.h`, `src/nodes/audio/AudioMixer.h`
- Pattern: Strategy / Component.

**Connection:**
- Purpose: Lightweight link between node IDs.
- Examples: `src/core/Graph.h` (Connection struct).
- Pattern: Data structure (POD).

**Session:**
- Purpose: The "Project" or "Patch" context.
- Examples: `src/core/Session.h`
- Pattern: Facade / Mediator.

## Entry Points

**C++ Entry:**
- Location: `src/main.cpp`
- Triggers: OS process start.
- Responsibilities: Initialize window, start openFrameworks main loop.

**Logic Setup:**
- Location: `src/ofApp.cpp` (setup)
- Triggers: App start.
- Responsibilities: Initialize `Session`, `ScriptBridge`, and register node types.

**Scripting Entry:**
- Location: `bin/data/scripts/main.lua`
- Triggers: `ScriptBridge::runScript()` or live-reload event.
- Responsibilities: Programmatically build or update the graph topology.

## Error Handling

**Strategy:** Fail-soft with logging.

**Patterns:**
- **JSON Safety:** `Node::getSafeJson` provides default values for missing or malformed keys.
- **Lua Error Catching:** `ScriptBridge` implements `ofxLuaListener` to log scripting errors without crashing.
- **ID Validation:** Graph operations validate `nodeId` existence before attempting connections.

## Cross-Cutting Concerns

**Logging:** Uses `ofLog` (openFrameworks standard).
**Validation:** `Graph::validateTopology()` for cycle detection (reserved).
**Authentication:** Not applicable (local application).

---

*Architecture analysis: 2026-02-21*
