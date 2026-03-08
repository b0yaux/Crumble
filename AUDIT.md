# Crumble — Deep Architecture Audit

**Scope:** Full codebase review following the Shadow Processor refactor.
**Goal:** Identify bugs, overcomplications, dead code, and modularity gaps.
**Method:** All source files read in full; findings are cross-referenced to specific lines.

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Architecture Strengths](#2-architecture-strengths)
3. [Critical Bugs](#3-critical-bugs)
4. [Structural Overcomplications](#4-structural-overcomplications)
5. [Dead Code Inventory](#5-dead-code-inventory)
6. [Pattern & Modulation System](#6-pattern--modulation-system)
7. [Node Modularity Analysis](#7-node-modularity-analysis)
8. [Thread Safety Analysis](#8-thread-safety-analysis)
9. [Cleanup Roadmap](#9-cleanup-roadmap)

---

## 1. Executive Summary

The Shadow Processor architecture is conceptually sound and the recent fixes to
pattern routing, connection stability, and the `_autoIndices` idempotency bug
were the right moves. The core data flow — Lua → Node → SPSC Queue →
Shadow Processor — works correctly for audio and video.

However, the refactor left behind a layer of **accretion**: legacy fields that
were never removed, temporary patterns that became permanent, and a handful of
real bugs that were not caught because the happy path works. The codebase is
close to clean; it needs a targeted pass, not another large refactor.

**Severity counts:**

| Severity | Count |
|---|---|
| 🔴 Critical bug (broken behavior) | 4 |
| 🟠 Real bug (wrong in edge cases) | 6 |
| 🟡 Overcomplication / architectural smell | 8 |
| ⚪ Dead code / cleanup | 9 |

---

## 2. Architecture Strengths

These are done correctly and should not be touched.

### 2.1 Wait-Free Audio Path

`Session::audioOut()` processes SPSC queue commands first, then iterates
`activeAudioProcessors` without any lock. `NodeProcessor::patternMap` and
`valuesMap` are exclusively owned by the audio thread (written only inside
`audioOut()`). This is genuinely lock-free and correct.

### 2.2 Pattern System Design

`Patterns.h` + `PatternMath.h` are a clean, composable, stateless functional
graph. Every pattern is a pure `cycle → value` function. The `getSignature()`
method enables future idempotency optimisation. The Lua `makeGen` wrapper with
`__mul`/`__add` metamethods is elegant and correct.

### 2.3 `sendCommand()` Dual-Queue Routing

`Session::sendCommand()` inspects both `audioProcessor` and `videoProcessor`
fields and routes the command to the appropriate queue (or both). A single
`REMOVE_NODE` command from `Node::~Node()` correctly tears down both shadow
sides. The logic is sound.

### 2.4 `_autoIndices` Global Fix

Making `_autoIndices` a global (`_G._autoIndices`) so it survives across script
re-runs is correct. The previous local-scoped version caused connection stacking
on every save. The current implementation properly resets only inside the
explicit `clear()` call.

### 2.5 `AssetRegistry` / `AssetCache` Separation

The two-layer media system (logical names → `AssetRegistry`, RAM
deduplication → `AssetCache`) is a clean design. The template `get<T>()` method
on `AssetCache` is particularly well-structured.

### 2.6 `Node::setupProcessor()` Guard

```
// Node.cpp
if (processor || audioProcessor || videoProcessor) return;
```

This guard prevents double-initialisation in all paths. The override pattern in
`AVSampler::setupProcessor()` (setting child `nodeId` before calling
`Node::setupProcessor()`) is the correct solution for composite nodes.

---

## 3. Critical Bugs

These are confirmed incorrect behaviors.

---

### 🔴 Bug #1 — `AudioMixer` and `SpeakersOutput` call `setupProcessor()` in their constructors, before `nodeId` is assigned

**Files:** `src/nodes/AudioMixer.cpp`, `src/nodes/SpeakersOutput.cpp`

```
// AudioMixer.cpp
AudioMixer::AudioMixer() {
    type = "AudioMixer";
    parameters->add(masterGain.set("masterGain", 1.0, 0.0, 4.0));
    numActiveInputs = 0;
    setupProcessor();   // ← called here, nodeId is still -1
}
```

```
// SpeakersOutput.cpp
SpeakersOutput::SpeakersOutput() {
    type = "SpeakersOutput";
    parameters->add(masterVolume.set("masterVolume", 1.0, 0.0, 1.0));
    setupProcessor();   // ← same problem
}
```

`Graph::createNode()` sets `node->nodeId` **after** the constructor returns:

```
// Graph.cpp
auto node = it->second();          // constructor runs here, nodeId = -1
node->nodeId = Node::nextNodeId.fetch_add(1);  // assigned here
ptr->setupProcessor();             // guard fires: processor already exists, returns
```

Because `setupProcessor()` has a guard (`if (processor || ...) return`), the
second call from `Graph::createNode()` is a no-op. The processor is permanently
stamped with `nodeId = -1`.

This does not break runtime processing (the `alive()` check in `audioOut()`
uses pointer identity, not `nodeId`). But it makes all debug logging for these
two node types useless, breaks any future `nodeId`-based processor lookup, and
is architecturally wrong.

**Fix:** Remove `setupProcessor()` calls from these constructors, matching the
pattern used by `VideoFileSource` and `AudioFileSource`, which explicitly note:
```
// NOTE: setupProcessor() is NOT called here.
// Graph::createNode() -> ptr->setupProcessor() handles it.
```

---

### 🔴 Bug #2 — `Graph::getContainingNode()` returns the wrong object, breaking `Inlet` and `Outlet`

**Files:** `src/core/Graph.cpp`, `src/nodes/subgraph/Inlet.cpp`

```
// Graph.cpp
Graph* Graph::getParentGraph()    const { return graph; }
Node*  Graph::getContainingNode() const { return graph; } // ← WRONG, same as above
```

Both methods return `this->graph` (the parent graph). They should return
different things. `Inlet::processVideo()` relies on the distinction:

```
// Inlet.cpp
Graph* childGraph     = dynamic_cast<Graph*>(graph);    // graph that owns this Inlet
Node*  containingNode = childGraph->getContainingNode(); // should be childGraph itself
Graph* parentGraph    = childGraph->getParentGraph();    // should be childGraph->graph

auto inputs = parentGraph->getInputConnections(containingNode->nodeId);
```

For the lookup to work, `containingNode->nodeId` must be the `nodeId` of the
`Graph` node as it appears in the **parent** graph's node list — i.e., `childGraph`
itself. But with the current implementation, `containingNode = childGraph->graph`
(the parent graph), so `containingNode->nodeId` is the parent graph's ID. The
connection lookup finds nothing or finds the wrong connections.

**Fix:**

```
Node* Graph::getContainingNode() const { return const_cast<Graph*>(this); }
```

---

### 🔴 Bug #3 — `AVSampler` does not propagate `loop` and `playing` changes to the shadow processor

**File:** `src/nodes/AVSampler.cpp`

```
// AVSampler.cpp — onParameterChanged
} else if (paramName == "loop") {
    if (cachedAudioLoop) cachedAudioLoop->set(loop.get());
    if (cachedVideoLoop) cachedVideoLoop->set(loop.get());
    // ← audioSource.onParameterChanged("loop") is NEVER called
    // ← shadow processor valuesMap["loop"] is never updated

} else if (paramName == "playing") {
    if (cachedAudioPlaying) cachedAudioPlaying->set(playing.get());
    if (cachedVideoPlaying) cachedVideoPlaying->set(playing.get());
    // ← same omission
}
```

The `cachedAudioLoop->set()` call updates the `ofParameter` value on the
`AudioFileSource` node object, but `AudioFileSource` has no `ofParameter`
listener for `loop`. The shadow processor's `valuesMap["loop"]` was seeded
at construction time and is never updated again.

`AudioFileProcessor::process()` calls `getParam("loop")` on every audio block:

```
// AudioFileSource.cpp
bool loop = getParam("loop") > 0.5f;
```

So a `smp.loop = false` command from Lua has no effect on audio playback.

**Fix:** Add the missing propagation calls:

```
} else if (paramName == "loop") {
    if (cachedAudioLoop) cachedAudioLoop->set(loop.get());
    if (cachedVideoLoop) cachedVideoLoop->set(loop.get());
    audioSource.onParameterChanged("loop");    // add
    videoSource.onParameterChanged("loop");    // add

} else if (paramName == "playing") {
    if (cachedAudioPlaying) cachedAudioPlaying->set(playing.get());
    if (cachedVideoPlaying) cachedVideoPlaying->set(playing.get());
    audioSource.onParameterChanged("playing"); // add
}
```

---

### 🔴 Bug #4 — `Transport` unit is "beats", but the README and pattern API describe it as "bars/loops"

**Files:** `src/core/Transport.cpp`, `README.md`

```
// Transport.cpp
double cyclesPerSecond = bpm / 60.0;  // at 120 BPM → 2 cycles/s
cycle += dt * cyclesPerSecond;
while (cycle >= 1.0) cycle -= 1.0;   // wraps once per BEAT
```

At 120 BPM this wraps 2× per second — once per beat. A 4/4 bar contains 4
beats. The README states:

> `osc(f)` — Sine wave (frequency in cycles-per-bar)
> `osc(1.0)` — repeats once per bar/loop

But with the current Transport, `osc(1.0)` completes one cycle per **beat**,
not per bar. At 120 BPM it oscillates 2 Hz. This contradicts the documented
behaviour and will confuse users writing rhythmic patterns.

The patterns themselves are mathematically unaffected (sine handles any
absolute cycle value), but the semantic contract is broken.

**Fix (Option A — change Transport to bars):**

```
// Transport.cpp
double beatsPerSecond = bpm / 60.0;
double barsPerSecond  = beatsPerSecond / 4.0; // assume 4/4
cycle += dt * barsPerSecond;
```

**Fix (Option B — rename the concept consistently):**

Rename the README entry to "cycles-per-beat" and align all documentation. Less
disruptive if you have existing scripts tuned to the current behavior.

---

## 4. Structural Overcomplications

These are not broken, but they add cognitive weight and should be simplified.

---

### 🟠 Issue #5 — Legacy `processor` field on `Node` is dead weight

**File:** `src/core/Node.h`

```
// Node.h
crumble::NodeProcessor* processor = nullptr; // Legacy
crumble::AudioProcessor* audioProcessor = nullptr;
crumble::VideoProcessor* videoProcessor = nullptr;
```

The `processor` field is the pre-refactor base-class shadow pointer. After
introducing `audioProcessor` and `videoProcessor`, `processor` is never given a
meaningful value by any current node — `createProcessor()` returns `nullptr` on
every concrete class. Yet it is:

- Included in every `ADD_NODE` and `REMOVE_NODE` command
- Checked in `Node::setupProcessor()` guard
- Forwarded in `Node::pushCommand()`
- Passed around in `AudioCommand`
- Given a "legacy fallback" warning path in `Session::sendCommand()`

This creates the illusion that a third processor type exists. Every time
someone reads `AudioCommand`, they must reason about three pointer fields
instead of two.

**Fix:** Remove `processor`, `createProcessor()`, `NodeProcessor* processor`
from `Node.h/cpp`, remove the legacy field from `AudioCommand`, and remove the
legacy warning branch from `Session::sendCommand()`.

---

### 🟠 Issue #6 — `AudioCommand` name is misleading; it handles video too

**File:** `src/core/AudioCommand.h`

```
// Session.h
crumble::SPSCQueue<crumble::AudioCommand> videoCommandQueue{1024};
```

`AudioCommand` is the message type for **both** the audio and video queues.
Reading `videoCommandQueue.try_dequeue(cmd)` where `cmd` is of type
`AudioCommand` is actively confusing. The name implies audio-only.

**Fix:** Rename to `EngineCommand` or `ShadowCommand`. This is a pure rename
with no behavioral change.

---

### 🟠 Issue #7 — `AudioMixer::sumBuf` is heap-allocated inside the processing loop

**File:** `src/nodes/AudioMixer.cpp`

```
// AudioMixerProcessor::process()
for (int i = 0; i < 16; i++) {
    auto& input = inputs[i];
    if (input.processor) {
        if (sumBuf.size() != buffer.size()) {
            sumBuf.allocate(...); // ← potential heap alloc on the audio thread
        }
```

`ofSoundBuffer::allocate()` calls `std::vector::resize()`. In steady state the
size never changes so this branch never executes, but it is checked 16× per
audio block. More importantly, on the first block after a topology change
(when a new node is connected), this **will** allocate on the audio thread,
causing a potential stall.

**Fix:** Allocate `sumBuf` once in `AudioMixerProcessor`'s constructor or in a
`prepare()`-equivalent, not in the hot path.

---

### 🟠 Issue #8 — `VideoFileProcessor` holds a raw pointer to `ofxHapPlayer` which lives on the Node

**Files:** `src/nodes/VideoFileSource.cpp`, `src/nodes/VideoFileSource.h`

```
// VideoFileSource.cpp
class VideoFileProcessor : public crumble::VideoProcessor {
public:
    VideoFileProcessor(ofxHapPlayer* p) : playerRef(p) {}
    ...
private:
    ofxHapPlayer* playerRef = nullptr; // ← raw pointer into the owning Node
};
```

`ofxHapPlayer player` is a member of `VideoFileSource` (the Node). The
processor is owned by the shadow system (deleted via `videoReleaseQueue`).
The Node is owned by the Graph. Their lifetimes are managed independently.

If `VideoFileSource` is destroyed (Graph removes it) while the
`videoReleaseQueue` has not yet processed the `REMOVE_NODE` dequeue, the
processor outlives the player and `playerRef` becomes a dangling pointer.
In practice this window is extremely short (one `Session::update()` frame), but
it is a use-after-free.

The same pattern exists in the audio side via the `data` pointer in
`AudioFileProcessor`:

```
// AudioFileSource.cpp
const float* data = nullptr; // raw pointer to sharedLoader's buffer
```

Here `sharedLoader` is held by `AudioFileSource` as a `shared_ptr`, and the
pointer is only sent to the processor **after** load. If the node is destroyed
while the processor is still being processed, `data` is dangling.

**Fix (short term):** Ensure `REMOVE_NODE` is the last command sent from the
destructor, and that the processor stops reading `data` when `totalSamples`
is set to 0. A `RELEASE_BUFFER` command (already in `AudioCommand::Type`)
could be sent first to zero out `data`.

**Fix (long term):** For audio, share the buffer through a `shared_ptr` sent
via the command (the `sharedLoader` already is a `shared_ptr`; pass it into
the processor via the command so the processor keeps it alive).

---

### 🟠 Issue #9 — `Graph::validateTopology()` uses a `vector::erase(begin())` as a queue (O(n²))

**File:** `src/core/Graph.cpp`

```
// Graph.cpp — validateTopology()
std::vector<int> queue;
...
while (!queue.empty()) {
    int currentNodeId = queue.front();
    queue.erase(queue.begin()); // ← O(n) erase repeated n times
```

This is Kahn's algorithm with an accidentally quadratic inner loop. For small
graphs (< 20 nodes) this is irrelevant. For larger generative setups it could
be noticeable.

**Fix:** Replace `std::vector<int> queue` with `std::deque<int> queue` and
`queue.pop_front()`.

---

### 🟠 Issue #10 — `Graph::processAudio()` and `processVideo()` use string type checks

**File:** `src/core/Graph.cpp`

```
// Graph.cpp
if (node && node->type == "Outlet") {
    Outlet* outlet = static_cast<Outlet*>(node); // ← unsafe static_cast after string check
```

`static_cast` without a prior `dynamic_cast` is undefined behavior if the
string check is ever wrong (e.g., a user-created node named "Outlet" of a
different type). The string `type` field is set by user code and the Lua
interpreter, making it untrusted for casting.

**Fix:** Use `dynamic_cast<Outlet*>(node.get())` and check for null.

---

### 🟡 Issue #11 — `AVSampler::onParameterChanged()` sends duplicate shadow commands for `speed`

**File:** `src/nodes/AVSampler.cpp`

```
// AVSampler.cpp — onParameterChanged("speed")
audioSource.modulate("speed", pat);
audioSource.onParameterChanged("speed");  // sends SET_PARAM + SET_PATTERN to audioProcessor
videoSource.modulate("speed", pat);
videoSource.onParameterChanged("speed");  // sends SET_PARAM + SET_PATTERN to videoProcessor

// ... (falls through to) ...

Node::onParameterChanged(paramName); // ← also sends SET_PARAM + SET_PATTERN to AVSampler's
                                     //    own audioProcessor, which IS audioSource's processor
```

`AVSampler::audioProcessor` is the same object as `audioSource.audioProcessor`
(set in `createAudioProcessor()`). The final `Node::onParameterChanged()` call
sends a second `SET_PARAM` and `SET_PATTERN` to the same processor. The
commands are idempotent so nothing breaks, but the queue receives two redundant
messages per parameter change.

**Fix:** Guard the final `Node::onParameterChanged()` call at the bottom of
`AVSampler::onParameterChanged()` to only fire for parameters that are
**not** manually handled above:

```
// Only propagate AVSampler-level params not already routed
if (paramName == "active" || paramName == "opacity") {
    Node::onParameterChanged(paramName);
}
```

---

### 🟡 Issue #12 — `Node::getPattern()` is not mutex-protected

**File:** `src/core/Node.cpp`

```
// Node.cpp
std::shared_ptr<Pattern<float>> Node::getPattern(const std::string& paramName) const {
    auto it = modulators.find(paramName); // ← no lock
    if (it != modulators.end()) return it->second;
    return nullptr;
}
```

`modulators` is protected by `modMutex` in `modulate()`, `clearModulator()`,
`prepare()`, and `onParameterChanged()` — but not in `getPattern()`. This is
called from `AVSampler::onParameterChanged()` on the main thread:

```
auto pat = getPattern("speed"); // ← reads modulators without lock
audioSource.modulate("speed", pat);
```

In practice both callers are on the main thread so there is no actual data
race today. But the inconsistency means any future call from a different context
(e.g., Lua coroutines, an update callback) would silently introduce a race.

**Fix:** Add `std::lock_guard<std::recursive_mutex> lock(modMutex)` to
`getPattern()`.

---

## 5. Dead Code Inventory

These can be deleted without behavioral change.

---

### ⚪ Dead Code #1 — `Node::modulatorCache` is declared but never written or read

**File:** `src/core/Node.h`

```
// Node.h
struct CacheEntry {
    ofSoundBuffer* buffer = nullptr;
    std::shared_ptr<Pattern<float>> pattern;
};
mutable std::unordered_map<void*, CacheEntry> modulatorCache;
```

`modulatorCache` appears in no `.cpp` file. It was presumably an earlier design
for caching modulated control buffers by parameter pointer. The current design
uses `controlBuffers` (keyed by parameter name string) instead.

**Delete:** The struct and the field.

---

### ⚪ Dead Code #2 — `Graph::validateTopology()` builds an `UPDATE_TOPOLOGY` command it never sends

**File:** `src/core/Graph.cpp`

```
// Graph.cpp
crumble::AudioCommand cmd;
cmd.type = crumble::AudioCommand::UPDATE_TOPOLOGY;
// Logic for passing full traversal list would go here
// For now, we'll keep it simple
```

The command is constructed and then falls out of scope without being enqueued.
`UPDATE_TOPOLOGY` is also never handled in `Session::audioOut()`.

**Delete:** The command construction block and the `UPDATE_TOPOLOGY` enum value.

---

### ⚪ Dead Code #3 — `VideoProcessor` has raw GL FBO fields that are set but never read

**File:** `src/core/NodeProcessor.h`

```
// NodeProcessor.h — VideoProcessor
unsigned int fbo_A = 0;
unsigned int fbo_B = 0;
unsigned int writeFbo = 0;
```

`swapFbo()` writes `writeFbo`:

```
writeFbo = (writeTex == &tex_A) ? fbo_A : fbo_B;
```

But `VideoMixerProcessor` (the only `VideoProcessor` subclass) uses `ofFbo
fboA, fboB` as private members and never reads `fbo_A`, `fbo_B`, or `writeFbo`
from the base class. These were placeholders for a planned raw-GL render path.

**Delete:** All three fields from `VideoProcessor`. If a raw-GL path is needed
later, it can be added to the specific processor that needs it.

---

### ⚪ Dead Code #4 — `Node::createProcessor()` / `NodeProcessor*` legacy path

Covered in Issue #5. The entire legacy single-processor path
(`createProcessor()`, `Node::processor`, the `processor` field in
`AudioCommand`, the warning log in `Session::sendCommand()`) is unused.

**Delete:** All of it.

---

### ⚪ Dead Code #5 — `AudioCommand::RELEASE_BUFFER` is defined but never sent

**File:** `src/core/AudioCommand.h`

```
LOAD_BUFFER,
RELEASE_BUFFER   // ← defined but no code sends this type
```

Neither `Session::audioOut()` nor `Session::update()` handle this type.

**Keep the type** (it's the right fix for Issue #8), but either implement it or
add a `// TODO` comment so it doesn't silently disappear.

---

### ⚪ Dead Code #6 — `Graph::outputFbo` private member in `VideoMixer` never used

**File:** `src/nodes/VideoMixer.h`

```
// VideoMixer.h
ofFbo outputFbo; // ← never referenced in VideoMixer.cpp
```

The FBO rendering moved entirely into `VideoMixerProcessor`. `outputFbo` on the
Node side was not cleaned up.

**Delete:** The field.

---

### ⚪ Dead Code #7 — `AudioMixer::tempBuffer` is declared but `sumBuf` (in the processor) is used instead

**File:** `src/nodes/AudioMixer.h`

```
// AudioMixer.h
ofSoundBuffer tempBuffer; // ← never referenced in AudioMixer.cpp
```

The actual summation buffer is `AudioMixerProcessor::sumBuf` (private to the
processor). The Node-level `tempBuffer` is a leftover.

**Delete:** The field.

---

### ⚪ Dead Code #8 — Leftover `print()` debug statement in Lua helper

**File:** `src/core/Interpreter.cpp`

```
// bindSessionAPI() helper string
print("CONNECT: " .. tostring(sId) .. " -> " .. tostring(dId) .. " toInput=" .. tostring(targetIn))
```

This fires on every `connect()` call from Lua and writes to stdout.

**Delete:** The `print(...)` line.

---

### ⚪ Dead Code #9 — `Graph::allocateFbo()` is a stub that does nothing

**File:** `src/nodes/VideoMixer.cpp`

```
// VideoMixer.cpp
void VideoMixer::allocateFbo() {
    // Moved to VideoMixerProcessor
}
```

The method is declared in `VideoMixer.h` (implicitly via the private section)
and defined as an empty body. It is never called.

**Delete:** Declaration and definition.

---

## 6. Pattern & Modulation System

### 6.1 Overall Assessment

The pattern system is the most modular part of the codebase. `Patterns.h` and
`PatternMath.h` have zero dependencies on any Crumble type — they only depend
on `<cmath>` and `<memory>`. This is ideal. Any new pattern type can be added
by adding a class to `PatternMath.h` with no changes to any other file.

The Lua `makeGen` / `parsePattern` bridge is clean. The recursive `parsePattern`
function correctly handles all combinator types and will return a meaningful
`nullptr` for unknown types rather than crashing.

### 6.2 Pattern Signature Strings

`std::to_string(float)` produces strings like `"osc:0.500000"`. These are
valid for equality comparison (same frequency → same signature) but are
fragile for serialization and produce poor debug output. Consider using a fixed
precision format:

```
// Example for Osc::getSignature()
return "osc:" + std::to_string((int)(freq * 10000));
```

Or use `fmt::format` / `snprintf` with `%.4g` for human-readable precision.

### 6.3 Missing `Constant` Modulator Path

When a user writes `smp.speed = 1.5`, `lua_setParam` is called which triggers
`node->clearModulator("speed")` followed by a plain `SET_PARAM`. This is
correct.

When a user writes `smp.speed = osc(0.5)`, `lua_setGenerator` is called which
calls `node->modulate()` then `onParameterChanged()` which sends `SET_PATTERN`.
This is also correct.

However, there is no path for the case where a user embeds a constant inside a
pattern composition and later wants to revert. `clearModulator()` correctly
sends `nullptr` to erase the pattern from the processor's `patternMap`, so the
fallback to `valuesMap` takes over. This round-trip is correct.

### 6.4 Pattern Serialization is Missing

Patterns assigned via `smp.speed = osc(0.5)` are **not serialized** by
`Node::serialize()`. When the graph is saved to `main.json` and reloaded, all
pattern modulations are lost. Only scalar parameter values survive the
save/load cycle.

This is a known gap (not introduced by the refactor). The `getSignature()`
method on each pattern was presumably designed with serialization in mind.

**Recommended path:** Store the Lua expression string (or the serialized pattern
tree using `getSignature()`) in a separate `modulators` JSON object:

```json
{
  "params": { "speed": 1.0 },
  "modulators": { "speed": "osc:0.5" }
}
```

On load, re-parse the signature string back into a `Pattern` object. The
`getSignature()` format already encodes the full tree recursively, so this is
straightforward.

---

## 7. Node Modularity Analysis

### 7.1 What is well-decoupled

- `Patterns.h` / `PatternMath.h` — zero Crumble dependencies ✅
- `AudioFileSource` — only depends on `Node.h`, `NodeProcessor.h`, `AudioCommand.h` ✅
- `VideoFileSource` — only depends on `Node.h`, `NodeProcessor.h` ✅
- `AudioMixer` / `VideoMixer` — only depend on `Node.h`, `NodeProcessor.h` ✅
- `Transport` — zero dependencies, pure math ✅

### 7.2 Where coupling is unnecessarily tight

**`Node.cpp` includes `Session.h`:**

```
// Node.cpp
#include "Session.h"
```

This is needed only for `g_session` access in `pushCommand()` and `~Node()`.
Every Node subclass transitively depends on the entire Session. This prevents
Node from ever being tested or used without a full Session.

**Fix:** Extract the `g_session` global into a minimal `EngineContext.h` that
exposes only `sendCommand()`. Nodes should not need to know about
`Session`, `Graph`, `AssetCache`, etc.

**`AVSampler` knows about `VideoFileSource`'s internal `clockMode`:**

```
// AVSampler.cpp
videoSource.parameters->get("clockMode").cast<int>().set(VideoFileSource::INTERNAL);
```

This reaches into the child node's parameter group by name and casts it to a
`VideoFileSource`-specific enum. This is tight coupling. If `VideoFileSource`
renames or removes `clockMode`, `AVSampler` breaks silently at runtime (the
`contains()` check only logs a warning).

**Fix:** Add an explicit `setClockMode(int)` method to `VideoFileSource`, or
handle clock mode internally based on whether the speed is being externally
modulated.

**`AVSampler` stores raw `ofParameter<float>*` cache pointers into child nodes:**

```
// AVSampler.h
ofParameter<float>* cachedAudioSpeed  = nullptr;
ofParameter<float>* cachedVideoSpeed  = nullptr;
ofParameter<float>* cachedAudioVolume = nullptr;
...
```

These are direct pointers into `AudioFileSource::speed` and
`VideoFileSource::speed`. If either child node is ever rebuilt or its parameter
group changes structure (e.g., in `deserialize()`), these become dangling. The
performance benefit (avoiding string lookup) is marginal — `ofParameterGroup`
lookup is a linear scan over typically 5-8 items.

**Recommendation:** Remove the pointer cache. Call
`audioSource.speed.set(speed.get())` directly. The minor string-lookup cost is
negligible compared to the fragility of raw parameter pointers.

### 7.3 `Inlet` and `Outlet` are not integrated with the shadow processor system

`Inlet::processAudio()` calls `source->pullAudio()` which calls the
Node-side `processAudio()` method. For `AudioFileSource` used standalone,
that method is intentionally a no-op (DSP is in the shadow processor). So
audio through a subgraph via `Inlet`/`Outlet` is **silent**.

The same problem exists on the video side, though `Inlet::processVideo()`
calls `source->getVideoOutput()` which for `VideoMixer` does correctly fetch
from the shadow processor's atomic texture pointer. Video subgraphs may work
by accident; audio subgraphs do not.

This is a fundamental architectural gap. Subgraphs were designed for the
legacy Node-processing path and have not been updated for the shadow system.

**Fix (short term):** Document this limitation clearly. Mark `Inlet`/`Outlet`
as "video-only" until the audio path is resolved.

**Fix (long term):** `Inlet` and `Outlet` need shadow processors that bridge
the parent and child processor graphs. The `Inlet`'s audio processor should
pull directly from the parent `AudioProcessor`'s output slot, which requires
the parent graph to pass a processor reference down into the child graph's
command queue at connection time.

### 7.4 `AudioFileSource` does not use `AssetCache`

```
// AudioFileSource.cpp
void AudioFileSource::load(const std::string& p) {
    if (!sharedLoader) sharedLoader = std::make_shared<ofxAudioFile>();
    sharedLoader->load(p);
```

Every `AudioFileSource` instance loads its own copy of the file into RAM.
`AssetCache` (which exists precisely to deduplicate this) is never called.
Two `AVSampler` nodes playing the same clip will hold two full RAM buffers.

`AssetCache::get<ofxAudioFile>()` already handles path resolution and
deduplication correctly. The fix is a one-line change in `load()`:

```
// Use shared cache instead of a per-node loader
auto cached = g_session->getCache().get<ofxAudioFile>(resolvedPath);
if (cached && cached->loaded()) {
    sharedLoader = cached;
    // send LOAD_BUFFER command...
}
```

---

## 8. Thread Safety Analysis

### 8.1 Confirmed safe

| Component | Why it is safe |
|---|---|
| `activeAudioProcessors` | Only modified inside `audioOut()` (audio thread), via dequeued commands. Never touched from main thread directly. |
| `AudioProcessor::patternMap` | Written only inside `audioOut()` command processing; read only inside `process()`. Both are the audio thread. |
| `AudioProcessor::valuesMap` | Written via `std::atomic<float>::store()`; read via `load()`. Relaxed ordering is sufficient since both sides are on the same thread for mutations, and the atomic itself prevents torn reads. |
| `VideoProcessor::patternMap` | Written and read exclusively on the main thread (video commands processed in `Session::update()`, `processVideo()` called immediately after). No race. |
| `VideoProcessor::readyTex` | `std::atomic<ofTexture*>` with `acquire`/`release` ordering. `processVideo()` writes via `swapFbo()` with `acq_rel`; `getOutput()` reads with `acquire`. Correct. |
| `Pattern::eval()` | Stateless pure function. No shared mutable state. Safe to call from any thread simultaneously. |
| `AssetCache` | `std::mutex` wraps all access. Correct, though audio thread should never call it. |
| `Graph::audioMutex` | Protects `connections` and `nodes` during topology changes. Main thread only. The audio thread never accesses these structures (it uses `activeAudioProcessors`). |

### 8.2 Confirmed unsafe / needs attention

**`Node::modMutex` vs. audio thread access:**

`modMutex` is `mutable std::recursive_mutex`. It protects `modulators` and
`controlBuffers`. Both are accessed on the main thread only (Lua, `prepare()`,
`getControl()`). The audio thread never touches `Node` objects directly.
The mutex is therefore protecting against re-entrant main-thread calls (e.g.,
a Lua `update()` callback calling `modulate()` while `prepare()` is running).
This is correct but the mutex name suggests audio-thread protection — worth
a clarifying comment.

**`g_session` global:**

```
// Session.cpp
Session* g_session = nullptr;
```

Written during `Session` construction/destruction (main thread). Read from
`Node::~Node()` (main thread) and `Node::pushCommand()` (main thread).
Not thread-safe if a node destructor were ever called from a background
thread, but in the current architecture all node ownership is on the main
thread. Safe in practice, fragile by design.

**The `SPSC` queue is used as `MPSC` in one case:**

`moodycamel::ReaderWriterQueue` is documented as **single-producer**,
**single-consumer**. In Crumble, the audio release queue is written from the
audio thread and read from the main thread — genuinely SPSC. The audio command
queue is written from the main thread and read from the audio thread — also
SPSC. This is correct usage.

However, if `Node::~Node()` is ever called during graph serialization or from
an `std::thread` that is not the main thread, two producers would write to
`audioCommandQueue` simultaneously, which is undefined behavior for this queue
type. Currently this cannot happen, but it is a latent risk worth a comment.

### 8.3 The `Session::~Session()` processor leak

```
// Session.cpp
Session::~Session() {
    soundStream.stop();
    soundStream.close();
    g_session = nullptr;
    // ← activeAudioProcessors and activeVideoProcessors are never deleted
    // ← release queues are never drained
}
```

When the session is torn down (application exit), the Graph is destroyed first
(it's a member of Session, destroyed before the destructor body runs). Each
`Node::~Node()` sends a `REMOVE_NODE` command into `audioCommandQueue`. But
`soundStream` has already been stopped, so `audioOut()` never runs again and
those commands are never dequeued. The processors accumulate in
`audioCommandQueue` (unprocessed) and the ones already in
`activeAudioProcessors` are simply abandoned.

On application exit this is cleaned up by the OS, but it prevents clean
valgrind/ASAN runs and would matter in a multi-session scenario.

**Fix:**

```
Session::~Session() {
    soundStream.stop();
    soundStream.close();

    // Drain and delete all shadow processors
    for (auto* p : activeAudioProcessors) delete p;
    for (auto* p : activeVideoProcessors) delete p;
    activeAudioProcessors.clear();
    activeVideoProcessors.clear();

    crumble::AudioProcessor* ap = nullptr;
    while (audioReleaseQueue.try_dequeue(ap)) delete ap;

    crumble::VideoProcessor* vp = nullptr;
    while (videoReleaseQueue.try_dequeue(vp)) delete vp;

    g_session = nullptr;
}
```

Note: The `graph` member is destroyed **after** the destructor body, so
node destructors fire and enqueue into an already-stopped stream. The
simplest fix is to call `graph.clear()` explicitly at the **top** of the
destructor body, before stopping the stream, so the REMOVE_NODE commands
are processed while the audio thread is still running.

---

## 9. Cleanup Roadmap

Ordered by impact vs. effort. Each item is independent of the others.

### Pass 1 — Zero-risk deletions (30 min total)

These are pure deletions with no behavioral change.

| # | What | File | Action |
|---|---|---|---|
| D1 | `Node::modulatorCache` struct and field | `Node.h` | Delete |
| D2 | `AudioMixer::tempBuffer` field | `AudioMixer.h` | Delete |
| D3 | `VideoMixer::outputFbo` field | `VideoMixer.h` | Delete |
| D4 | `VideoProcessor::fbo_A/B/writeFbo` fields | `NodeProcessor.h` | Delete |
| D5 | `UPDATE_TOPOLOGY` command construction in `validateTopology()` | `Graph.cpp` | Delete the block |
| D6 | `allocateFbo()` stub declaration and definition | `VideoMixer.h/.cpp` | Delete |
| D7 | Lua `print("CONNECT: ...")` debug line | `Interpreter.cpp` | Delete |
| D8 | Legacy `processor` field, `createProcessor()`, and all related `AudioCommand`/`Session`/`Node` handling | `Node.h/cpp`, `AudioCommand.h`, `Session.cpp` | Delete entire legacy path |

### Pass 2 — Bug fixes (2–4 hours total)

| # | What | File | Effort |
|---|---|---|---|
| B1 | Remove `setupProcessor()` from `AudioMixer` and `SpeakersOutput` constructors | `AudioMixer.cpp`, `SpeakersOutput.cpp` | 5 min |
| B2 | Fix `Graph::getContainingNode()` to return `this` | `Graph.cpp` | 2 min |
| B3 | Add `audioSource.onParameterChanged("loop")` and `("playing")` in `AVSampler` | `AVSampler.cpp` | 5 min |
| B4 | Add `std::lock_guard` to `Node::getPattern()` | `Node.cpp` | 2 min |
| B5 | Replace `vector::erase(begin())` with `std::deque::pop_front()` in `validateTopology()` | `Graph.cpp` | 5 min |
| B6 | Replace `static_cast<Outlet*>` after string check with `dynamic_cast` | `Graph.cpp` | 5 min |
| B7 | Fix `Session::~Session()` to drain and delete processors | `Session.cpp` | 15 min |
| B8 | Decide and fix Transport cycle unit (beats vs. bars) and update README | `Transport.cpp`, `README.md` | 20 min |

### Pass 3 — Structural improvements (half-day)

| # | What | Benefit |
|---|---|---|
| S1 | Rename `AudioCommand` → `EngineCommand` throughout | Clarity for video path readers |
| S2 | Guard `AVSampler::onParameterChanged()` tail call to avoid duplicate shadow commands | Cleaner queue, easier to reason about |
| S3 | Pre-allocate `AudioMixerProcessor::sumBuf` in constructor instead of hot path | Removes conditional allocation from audio thread |
| S4 | Have `AudioFileSource::load()` use `AssetCache` | Deduplicates RAM buffers for repeated paths |
| S5 | Add `VideoFileSource::setClockMode(int)` and remove `AVSampler`'s direct parameter group access | Decouples AVSampler from VideoFileSource internals |
| S6 | Remove raw `cachedAudio*/cachedVideo*` pointer cache in `AVSampler` | Eliminates fragile pointers-into-child-parameters |

### Pass 4 — Feature gaps (when needed)

| # | What | Complexity |
|---|---|---|
| F1 | Pattern serialization (save/load modulations to JSON) | Medium |
| F2 | Fix `Inlet`/`Outlet` audio path to use shadow processors | High |
| F3 | Implement `RELEASE_BUFFER` command to safely zero processor's data pointer before Node destruction | Medium |

---

## Summary Table

| # | Severity | Description | File | Status |
|---|---|---|---|---|
| 1 | 🔴 Critical | `AudioMixer`/`SpeakersOutput` call `setupProcessor()` before `nodeId` assigned | `AudioMixer.cpp`, `SpeakersOutput.cpp` | **Unfixed** |
| 2 | 🔴 Critical | `Graph::getContainingNode()` returns parent instead of self — breaks Inlet/Outlet | `Graph.cpp` | **Unfixed** |
| 3 | 🔴 Critical | `AVSampler` doesn't propagate `loop`/`playing` to shadow processor | `AVSampler.cpp` | **Unfixed** |
| 4 | 🔴 Critical | Transport cycle unit is beats, README documents bars | `Transport.cpp` | **Unfixed** |
| 5 | 🟠 Bug | Legacy `Node::processor` field creates confusion and dead paths | `Node.h/cpp`, `AudioCommand.h` | **Unfixed** |
| 6 | 🟠 Bug | `AudioCommand` name is misleading for video use | `AudioCommand.h`, `Session.h` | **Unfixed** |
| 7 | 🟠 Bug | `AudioMixerProcessor::sumBuf` conditionally allocates on audio thread | `AudioMixer.cpp` | **Unfixed** |
| 8 | 🟠 Bug | `VideoFileProcessor` holds raw pointer into Node's player — potential dangling | `VideoFileSource.cpp` | **Unfixed** |
| 9 | 🟠 Bug | `validateTopology()` is O(n²) due to vector-as-queue | `Graph.cpp` | **Unfixed** |
| 10 | 🟠 Bug | `Graph::processAudio/Video()` uses unsafe `static_cast` after string check | `Graph.cpp` | **Unfixed** |
| 11 | 🟡 Smell | `AVSampler::onParameterChanged("speed")` sends duplicate shadow commands | `AVSampler.cpp` | **Unfixed** |
| 12 | 🟡 Smell | `Node::getPattern()` missing mutex | `Node.cpp` | **Unfixed** |
| 13 | ⚪ Dead | `Node::modulatorCache` declared, never used | `Node.h` | **Delete** |
| 14 | ⚪ Dead | `UPDATE_TOPOLOGY` command built but never sent | `Graph.cpp` | **Delete** |
| 15 | ⚪ Dead | `VideoProcessor::fbo_A/B/writeFbo` set but never read | `NodeProcessor.h` | **Delete** |
| 16 | ⚪ Dead | `Node::processor` / `createProcessor()` legacy path | `Node.h/cpp`, `AudioCommand.h` | **Delete** |
| 17 | ⚪ Dead | `RELEASE_BUFFER` enum value defined, never sent | `AudioCommand.h` | **Implement or document** |
| 18 | ⚪ Dead | `VideoMixer::outputFbo` never used | `VideoMixer.h` | **Delete** |
| 19 | ⚪ Dead | `AudioMixer::tempBuffer` never used | `AudioMixer.h` | **Delete** |
| 20 | ⚪ Dead | Lua `print("CONNECT: ...")` debug line | `Interpreter.cpp` | **Delete** |
| 21 | ⚪ Dead | `VideoMixer::allocateFbo()` is an empty stub | `VideoMixer.h/cpp` | **Delete** |
| — | Gap | `Inlet`/`Outlet` audio path bypasses shadow system — subgraph audio is broken | `Inlet.cpp` | **Document/Fix** |
| — | Gap | `AudioFileSource` does not use `AssetCache` — no RAM deduplication | `AudioFileSource.cpp` | **Fix** |
| — | Gap | Pattern modulations are not serialized to JSON | `Node.cpp`, `Patterns.h` | **Future work** |
| — | Gap | `Session::~Session()` leaks all shadow processors | `Session.cpp` | **Fix** |