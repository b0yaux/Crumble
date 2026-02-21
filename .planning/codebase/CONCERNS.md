# Codebase Concerns

**Analysis Date:** 2026-02-21

## Tech Debt

**Undo/Redo via JSON Snapshots:**
- Issue: `Session::undo()` and `redo()` take full JSON snapshots of the entire graph and store them in memory.
- Files: `src/core/Session.cpp`, `src/core/Session.h`
- Impact: Inefficient for large graphs; causes memory bloat and potential performance stuttering during checkpointing. Restoring from JSON clears and rebuilds the entire graph, likely causing visual flickers.
- Fix approach: Implement a Command pattern where only the changes (diffs) are stored in the undo stack.

**Recursive Graph Evaluation:**
- Issue: `Graph::pullFromNode()` uses recursion to traverse the graph for updates.
- Files: `src/core/Graph.cpp`
- Impact: Risks stack overflow for extremely deep or complex graphs.
- Fix approach: Use iterative topological sort and update nodes in linear order.

**Incomplete Memory Management in UI:**
- Issue: `GraphUI::nodes` map caches `NodeViz` data but never removes entries when nodes are deleted from the core graph.
- Files: `src/ui/GraphUI.cpp`, `src/ui/GraphUI.h`
- Impact: Memory leak of `NodeViz` structures and increasing iteration overhead in the UI layer over time.
- Fix approach: Synchronize the `GraphUI::nodes` map with the core graph state or prune missing nodes during the `draw()` call.

**Abandoned Files in `lost/`:**
- Issue: The project contains a `lost/` directory with 40MB of hashed `.txt` files of unknown origin.
- Files: `lost/*`
- Impact: Unnecessary bloat in the codebase; confusing for developers.
- Fix approach: Audit the contents and remove the directory if it's indeed temporary/corrupted data fragments.

## Known Bugs

**Static selectedLayer:**
- Issue: `selectedLayer` is initialized to 0 and never updated by any UI interaction.
- Files: `src/ofApp.h`, `src/ofApp.cpp`
- Impact: Pressing '-' always attempts to remove the first layer (index 0), regardless of user intent.
- Trigger: Press '-' or '_' in the application.
- Workaround: None, requires code fix to update `selectedLayer`.

**Incomplete Cycle Detection:**
- Issue: Kahn's algorithm is used in `validateTopology()` but only logs a warning instead of preventing or gracefully handling cycles.
- Files: `src/core/Graph.cpp`
- Impact: If a cycle is created, `pullFromNode` might enter infinite recursion (if not for the `lastUpdateFrame` guard) or produce inconsistent results.
- Trigger: Connect a node's output back to its input (directly or indirectly).

## Security Considerations

**Unsandboxed Lua Execution:**
- Risk: `ScriptBridge` exposes `_listDir` and `_exists` which allow Lua scripts to probe the filesystem. Since it uses `ofxLua`, scripts may have broad access to system resources.
- Files: `src/core/ScriptBridge.cpp`
- Current mitigation: None detected.
- Recommendations: Implement a sandbox for Lua execution and restrict filesystem access to the `bin/data` directory.

## Performance Bottlenecks

**Per-Frame Filesystem Polling:**
- Problem: `checkLiveReload()` calls `ofFile::doesFileExist` and `std::filesystem::last_write_time` every single frame for both JSON and Lua files.
- Files: `src/ofApp.cpp`
- Cause: Simple implementation of hot-reloading via polling.
- Improvement path: Use a platform-specific file watcher (like `FSEvents` on macOS or `inotify` on Linux) or poll at a lower frequency (e.g., once every 0.5s).

**Redundant Object Creation in VideoMixer:**
- Problem: `VideoMixer::update` creates a `RenderLayer` struct and a `std::vector` every frame.
- Files: `src/nodes/video/VideoMixer.cpp`
- Cause: Transient state management during the render loop.
- Improvement path: Reuse a member vector to avoid repeated allocations.

**Frame-Rate Dependent UI Physics:**
- Problem: `GraphUI::forceLayout` simulation speed is directly tied to the draw frame rate.
- Files: `src/ui/GraphUI.cpp`
- Cause: Multiplies forces by fixed constants without `dt` (delta time).
- Improvement path: Use `ofGetLastFrameTime()` to make physics calculations time-independent.

## Fragile Areas

**Hybrid State Management (Lua vs JSON vs UI):**
- Files: `src/ofApp.cpp`, `src/core/ScriptBridge.cpp`, `src/core/Session.cpp`
- Why fragile: The application supports state changes via Lua scripts (idempotent), JSON loading, and direct UI manipulation. These three systems can easily conflict (e.g., UI change overwritten by a script reload).
- Safe modification: Changes should ideally flow through the `CommandQueue` proposed in `docs/architecture_proposal.md`.
- Test coverage: Zero detected for the bridge/session interaction.

**VideoMixer Parameter Synchronization:**
- Files: `src/nodes/video/VideoMixer.cpp`
- Why fragile: Manual unrolling of parameters and "loose" type conversion in `deserialize` to avoid "Abort traps" indicates a fragile serialization layer.
- Safe modification: Use the proposed `Serializer` class to unify how parameters are handled across all node types.

---

*Concerns audit: 2026-02-21*
