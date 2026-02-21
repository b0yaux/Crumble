# Codebase Concerns

**Analysis Date:** 2026-02-21

## Tech Debt

**Undo/Redo via JSON Snapshots:**
- Issue: `Session::undo()` and `redo()` take full JSON snapshots of the entire graph and store them in memory.
- Files: `src/core/Session.cpp`, `src/core/Session.h`
- Impact: Inefficient for large graphs; causes memory bloat and potential performance stuttering during checkpointing. Restoring from JSON clears and rebuilds the entire graph, likely causing visual flickers.
- Fix approach: Implement a Command pattern where only the changes (diffs) are stored in the undo stack.

**Recursive Graph Evaluation (Resolved):**
- Issue was resolved by refactoring `Graph::update` to use a pre-computed `traversalOrder` based on a Topological Sort (Kahn's Algorithm). This eliminates the risk of stack overflow.

**Incomplete Memory Management in UI (Resolved):**
- Issue was resolved by implementing a reconciliation pass in `GraphUI::draw()` that prunes dead node IDs from the visual state map.

**Incomplete Cycle Detection (Resolved):**
- Issue was resolved by integrating `validateTopology()` directly into `Graph::connect()`. Connections that would create a cycle are now rejected.


## Legacy Code & Technical Debt

**Obsolete UI State in ofApp:**
- Issue: `ofApp` maintains `selectedLayer` and `removeVideoLayer()`, which are holdovers from an older UI paradigm.
- Files: `src/ofApp.h`, `src/ofApp.cpp`
- Impact: Confusing for developers; `selectedLayer` is always 0 and the `-` key shortcut is disconnected from the current `GraphUI` logic.
- Fix approach: Remove legacy members from `ofApp` and unify interaction logic within `GraphUI`.

**Unbounded Parameter Growth in VideoMixer (Resolved):**
- Issue was resolved by implementing parameter removal in `resizeLayerArrays`. Excess parameters are now removed from the `ofParameterGroup` when `numActiveLayers` decreases.


## Security Considerations

**Unsandboxed Lua Execution:**
- Risk: `ScriptBridge` exposes `_listDir` and `_exists` which allow Lua scripts to probe the filesystem. Since it uses `ofxLua`, scripts may have broad access to system resources.
- Files: `src/core/ScriptBridge.cpp`
- Current mitigation: None detected.
- Recommendations: Implement a sandbox for Lua execution and restrict filesystem access to the `bin/data` directory.

## Performance Bottlenecks

**None Detected.**
- Previous concern regarding per-frame filesystem polling was resolved by implementing a background `FileWatcher` thread.

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
