# Crumble Project - New Architecture Proposal

## Overview

After cleanup, the project now has a clean foundation. This document proposes a 3-layer architecture for UI decoupling, serialization, and extensibility.

---

## Cleaned Project Structure

```
Crumble/
├── src/
│   ├── core/                      # Core graph system
│   │   ├── Node.h                 # Base node (40 lines) - UNCHANGED
│   │   ├── Graph.h/.cpp           # Graph container (323 lines) - ENHANCE
│   │   ├── Serializer.h/.cpp      # NEW: JSON serialization layer
│   │   └── NodeFactory.h/.cpp     # NEW: Type registry for deserialization
│   ├── commands/                  # NEW: Command pattern layer
│   │   ├── Command.h              # Abstract command base
│   │   ├── CommandQueue.h/.cpp    # Queue with undo/redo
│   │   ├── AddNodeCmd.h/.cpp      # Add node command
│   │   ├── ConnectCmd.h/.cpp      # Connect nodes command
│   │   ├── SetParamCmd.h/.cpp     # Set parameter command
│   │   └── LoadPatchCmd.h/.cpp    # Load patch file command
│   ├── nodes/
│   │   └── video/                 # Video processing nodes
│   │       ├── VideoFileSource.h/.cpp  # UNCHANGED (157 lines)
│   │       ├── VideoMixer.h/.cpp       # UNCHANGED (471 lines)
│   │       └── ScreenOutput.h/.cpp     # UNCHANGED (73 lines)
│   ├── ui/                        # NEW: UI controllers
│   │   ├── ImGuiController.h/.cpp # ImGui parameter panels
│   │   ├── KeyboardController.h/.cpp   # Keyboard shortcuts
│   │   └── FileWatcher.h/.cpp     # Hot-reload file watcher
│   ├── ofApp.h/.cpp               # Main application - REFACTORED
│   └── main.cpp                   # Entry point - UNCHANGED
├── data/
│   └── patches/
│       └── main.json              # Patch files
├── docs/
│   ├── ui_decoupling_research.md  # Research document
│   └── architecture_proposal.md   # This file
└── [build files unchanged]
```

---

## 3-Layer Architecture (UML Component Diagram)

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Layer 3: UI Controllers                      │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │
│  │ ImGuiController │  │KeyboardController│  │   FileWatcher       │  │
│  │─────────────────│  │─────────────────│  │─────────────────────│  │
│  │+render()        │  │+onKeyPressed()  │  │+watch(path)         │  │
│  │+drawParamPanel()│  │+bindShortcut()  │  │+onFileChanged()     │  │
│  └────────┬────────┘  └────────┬────────┘  └──────────┬──────────┘  │
└───────────┼───────────────────┼──────────────────────┼─────────────┘
            │                   │                      │
            │  Commands         │  Commands            │  Commands
            ▼                   ▼                      ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       Layer 2: Command System                        │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                      CommandQueue                             │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐ │  │
│  │  │   Commands   │  │  Undo Stack  │  │    Redo Stack        │ │  │
│  │  │  [History]   │  │  [LIFO]      │  │    [LIFO]            │ │  │
│  │  └──────────────┘  └──────────────┘  └──────────────────────┘ │  │
│  │                                                               │  │
│  │  Operations:                                                  │  │
│  │    - push(std::unique_ptr<Command>)                           │  │
│  │    - execute(Command&)           → calls cmd.execute(graph)   │  │
│  │    - undo()                      → calls cmd.undo(graph)      │  │
│  │    - redo()                                                   │  │
│  │    - clear()                                                  │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  Command Hierarchy:                                                  │
│  ┌──────────────┐                                                    │
│  │   Command    │  <<abstract>>                                      │
│  │  (interface) │  execute(Graph&) = 0                               │
│  │              │  undo(Graph&) = 0                                   │
│  └──────┬───────┘                                                    │
│         │                                                            │
│    ┌────┼────┬─────────┬───────────┬──────────┐                      │
│    ▼    ▼    ▼         ▼           ▼          ▼                      │
│ ┌────┐┌────┐┌────────┐┌─────────┐┌────────┐┌─────────────┐          │
│ │Add ││Remo││Connect ││Disconnect││SetParam││ LoadPatch   │          │
│ │Node││ve  ││  Cmd   ││   Cmd    ││  Cmd   ││    Cmd      │          │
│ │Cmd ││Node││        ││          ││        ││             │          │
│ └────┘│Cmd │└────────┘└─────────┘└────────┘└─────────────┘          │
│       └────┘                                                         │
└──────────────────────────────────────────────────────────────────────┘
            │
            │  Executes on
            ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Layer 1: Graph Model                            │
│                                                                      │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                        Graph                                  │  │
│  │  <<Composite>>                                                │  │
│  │  inherits from Node                                           │  │
│  │                                                               │  │
│  │  - nodes: vector<unique_ptr<Node>>                            │  │
│  │  - connections: vector<Connection>                            │  │
│  │  - evaluationOrder: vector<int>                               │  │
│  │                                                               │  │
│  │  + addNode(type, name): Node*                                 │  │
│  │  + removeNode(index): void                                    │  │
│  │  + connect(from, to, fromOut, toIn): bool                     │  │
│  │  + disconnect(connectionIndex): void                          │  │
│  │  + update(): void                    // topological sort      │  │
│  │  + getVideoOutput(): Texture*                                 │  │
│  │  + toJson(): ofJson                    // NEW                 │  │
│  │  + fromJson(json): bool                // NEW                 │  │
│  └──────────┬────────────────────────────────────────────────────┘  │
│             │                                                        │
│             │ owns                                                   │
│             ▼                                                        │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                        Node                                   │  │
│  │  <<Abstract Base>>                                            │  │
│  │                                                               │  │
│  │  - nodeIndex: int                                             │  │
│  │  - nodeName: string                                           │  │
│  │  - parameters: ofParameterGroup                               │  │
│  │  - lastUpdateFrame: uint64_t                                  │  │
│  │                                                               │  │
│  │  + getVideoOutput(): Texture* = 0                             │  │
│  │  + update(): void = 0                                         │  │
│  │  + getParameters(): ofParameterGroup&                         │  │
│  └──────────┬────────────────────────────────────────────────────┘  │
│             │                                                        │
│             │ polymorphic                                            │
│      ┌──────┼──────┬─────────────┐                                   │
│      ▼      ▼      ▼             ▼                                   │
│ ┌─────────┐┌─────────┐┌─────────────────┐┌──────────────────────┐   │
│ │VideoFile││VideoMixer││   ScreenOutput  ││      Graph           │   │
│ │ Source  ││         ││                 ││   (nested!)          │   │
│ └─────────┘└─────────┘└─────────────────┘└──────────────────────┘   │
│                                                                      │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                     Serializer                                │  │
│  │                                                               │  │
│  │  + serializeGraph(graph): ofJson                              │  │
│  │  + deserializeGraph(json, graph): bool                        │  │
│  │  + saveToFile(graph, path): void                              │  │
│  │  + loadFromFile(path, graph): bool                            │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                     NodeFactory                               │  │
│  │                                                               │  │
│  │  - registry: map<string, function<Node*()>>                   │  │
│  │                                                               │  │
│  │  + registerType(name, factory): void                          │  │
│  │  + createNode(type): Node*                                    │  │
│  │  + getRegisteredTypes(): vector<string>                       │  │
│  └───────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Class Relationships (UML Class Diagram)

```
                                    ┌─────────────────────┐
                                    │   <<abstract>>      │
                                    │      Command        │
                                    │─────────────────────│
                                    │# graph: Graph*      │
                                    │─────────────────────│
                                    │+ execute() = 0      │
                                    │+ undo() = 0         │
                                    └──────────┬──────────┘
                                               │
                          ┌────────────────────┼────────────────────┐
                          │                    │                    │
               ┌──────────▼──────────┐  ┌─────▼──────┐  ┌───────────▼──────────┐
               │      AddNodeCmd     │  │ ConnectCmd │  │     SetParamCmd      │
               │─────────────────────│  │────────────│  │──────────────────────│
               │- nodeType: string   │  │- from: int │  │- nodeIndex: int      │
               │- nodeName: string   │  │- to: int   │  │- paramName: string   │
               │- createdIndex: int  │  │- fromOut:  │  │- value: float        │
               │─────────────────────│  │  int       │  │- oldValue: float     │
               │+ execute()          │  │- toIn: int │  │──────────────────────│
               │+ undo()             │  │────────────│  │+ execute()           │
               └─────────────────────┘  │+ execute() │  │+ undo()              │
                                        │+ undo()    │  └──────────────────────┘
                                        └────────────┘

┌─────────────────────┐         ┌─────────────────────┐         ┌─────────────────────┐
│       Graph         │◄────────│     CommandQueue    │◄────────│  ImGuiController    │
│─────────────────────│  uses   │─────────────────────│  uses   │─────────────────────│
│- nodes: vector<...> │         │- history: vector    │         │- queue: CommandQueue│
│- connections: ...   │         │- undoStack: stack   │         │- gui: ofxImGui      │
│─────────────────────│         │- redoStack: stack   │         │─────────────────────│
│+ toJson(): ofJson   │         │─────────────────────│         │+ render()           │
│+ fromJson(): bool   │         │+ push(cmd)          │         │+ drawParamPanel()   │
│+ addNode(): Node*   │         │+ execute(cmd)       │         │+ onParameterChange()│
│+ connect(): bool    │         │+ undo()             │         └─────────────────────┘
└──────────┬──────────┘         │+ redo()             │
           │                    └─────────────────────┘
           │ owns
           ▼
┌─────────────────────┐
│  <<abstract>> Node  │
│─────────────────────│
│- nodeIndex: int     │
│- nodeName: string   │
│- parameters: Group  │
│─────────────────────│
│+ update() = 0       │
│+ getVideoOutput()=0 │
└──────────┬──────────┘
           │
     ┌─────┴──────┬─────────────┐
     ▼            ▼             ▼
┌───────────┐ ┌───────────┐ ┌──────────────┐
│VideoMixer │ │VideoFile  │ │ScreenOutput  │
│───────────│ │  Source   │ │──────────────│
│-layers... │ │───────────│ │-x, y, w, h   │
│-shader    │ │-player    │ │──────────────│
│───────────│ │-audioFile │ │+ draw()      │
│+ update() │ │───────────│ └──────────────┘
│+ getOutput│ │+ update() │
└───────────┘ │+ getOutput│
              └───────────┘
```

---

## Sequence Diagram: Loading a Patch

```
User          FileWatcher    CommandQueue      Graph       Serializer   File System
 │                │               │              │              │              │
 │                │               │              │              │              │
 │                │  watch("patches/main.json")  │              │              │
 │                │◄──────────────│              │              │              │
 │                │               │              │              │              │
 │   [edit file]  │               │              │              │              │
 │────────────────│               │              │              │              │
 │                │               │              │              │              │
 │                │  onFileChanged()             │              │              │
 │                │──────────────►│              │              │              │
 │                │               │              │              │              │
 │                │               │  loadFromFile(path)         │              │
 │                │               │─────────────►│              │              │
 │                │               │              │              │              │
 │                │               │              │  read file   │              │
 │                │               │              │─────────────►│              │
 │                │               │              │              │              │
 │                │               │              │◄─────────────│              │
 │                │               │              │  json data   │              │
 │                │               │              │              │              │
 │                │               │              │  parse json  │              │
 │                │               │              │──────────────┼─────────────►│
 │                │               │              │              │              │
 │                │               │◄─────────────│              │              │
 │                │               │  graph data  │              │              │
 │                │               │              │              │              │
 │                │               │  create AddNodeCmd for each node
 │                │               │  create ConnectCmd for each connection
 │                │               │  create SetParamCmd for each param
 │                │               │              │              │              │
 │                │               │  push(commands)
 │                │               │─────────────►│              │              │
 │                │               │              │              │              │
 │                │               │  execute() on main thread
 │                │               │─────────────►│              │              │
 │                │               │              │              │              │
 │                │               │              │  graph rebuilt!
 │                │               │◄─────────────│              │              │
 │                │               │              │              │              │
 │                │◄──────────────│  done        │              │              │
 │                │               │              │              │              │
 │   [view live]  │               │              │              │              │
 │◄───────────────│               │              │              │              │
```

---

## Sequence Diagram: User Edits Parameter via ImGui

```
User    ImGuiController   CommandQueue      Graph        Node
 │            │               │              │            │
 │ interact   │               │              │            │
 │───────────►│               │              │            │
 │            │               │              │            │
 │            │ detects param change        │            │
 │            │ (opacity from 0.5 to 0.8)   │            │
 │            │               │              │            │
 │            │ create SetParamCmd          │            │
 │            │ (node=mixer, param="opacity", value=0.8)
 │            │               │              │            │
 │            │ push(cmd)     │              │            │
 │            │──────────────►│              │            │
 │            │               │              │            │
 │            │               │ execute(cmd) │            │
 │            │               │─────────────►│            │
 │            │               │              │            │
 │            │               │              │ set opacity│
 │            │               │              │───────────►│
 │            │               │              │            │
 │            │               │◄─────────────│  done      │
 │            │               │              │            │
 │            │◄──────────────│  cmd stored  │            │
 │            │   for undo    │              │            │
 │            │               │              │            │
 │  [see      │               │              │            │
 │   change]  │               │              │            │
 │◄───────────│               │              │            │
 │            │               │              │            │
 │ press      │               │              │            │
 │ Ctrl+Z     │               │              │            │
 │───────────►│               │              │            │
 │            │               │              │            │
 │            │ undo()        │              │            │
 │            │──────────────►│              │            │
 │            │               │              │            │
 │            │               │ cmd.undo()   │            │
 │            │               │ (restore 0.5)│            │
 │            │               │─────────────►│            │
 │            │               │              │            │
 │            │               │              │ reset param│
 │            │               │              │───────────►│
 │            │               │              │            │
 │  [see      │               │              │            │
 │   undone]  │               │              │            │
 │◄───────────│               │              │            │
```

---

## Implementation Phases

### Phase 1: Graph Serialization (Foundation) - 2-3 hours
**Goal**: Remove broken PatchLoader, add proper serialization

**Files to modify:**
- `src/core/Graph.h` - Add `toJson()` and `fromJson()` methods
- `src/core/Graph.cpp` - Implement serialization logic

**New files:**
- `src/core/NodeFactory.h/cpp` - Type registry for node creation
- `src/core/Serializer.h/cpp` - File I/O wrapper

**JSON Schema:**
```json
{
  "version": "1.0",
  "graph": {
    "nodes": [
      {
        "id": 0,
        "type": "VideoMixer",
        "name": "MainMixer",
        "params": {
          "numLayers": 4,
          "layerOpacity": [1.0, 0.5, 0.8, 1.0]
        }
      },
      {
        "id": 1,
        "type": "VideoFileSource",
        "name": "Video1",
        "params": {
          "file": "video1.mov",
          "loop": true
        }
      }
    ],
    "connections": [
      {"from": 1, "to": 0, "fromOutput": 0, "toInput": 0}
    ],
    "output": {
      "video": 0
    }
  }
}
```

### Phase 2: Command System - 4-6 hours
**Goal**: Enable UI decoupling and undo/redo

**New files:**
- `src/commands/Command.h` - Abstract base class
- `src/commands/CommandQueue.h/cpp` - Queue management with undo/redo
- `src/commands/AddNodeCmd.h/cpp` - Add node command
- `src/commands/ConnectCmd.h/cpp` - Connect nodes command
- `src/commands/SetParamCmd.h/cpp` - Set parameter command
- `src/commands/LoadPatchCmd.h/cpp` - Load patch command

**Refactor:**
- `src/ofApp.cpp` - Replace direct graph manipulation with commands

### Phase 3: File Watcher & Hot-Reload - 2-3 hours
**Goal**: Edit JSON, see changes immediately

**New files:**
- `src/ui/FileWatcher.h/cpp` - Watch file modifications

**Modify:**
- `src/ofApp.cpp` - Integrate file watcher, diff JSON changes

### Phase 4: ImGui Integration - 4-6 hours
**Goal**: Visual parameter editing

**Dependencies:**
- Add `ofxImGui` to `addons.make`

**New files:**
- `src/ui/ImGuiController.h/cpp` - Parameter panels
- `src/ui/KeyboardController.h/cpp` - Shortcut handling (extract from ofApp)

**Refactor:**
- `src/ofApp.cpp` - Use controllers instead of direct handling

---

## Benefits of This Architecture

| Feature | Before (PatchLoader) | After (3-Layer) |
|---------|---------------------|-----------------|
| **Single Responsibility** | ❌ Mixed parsing + building | ✅ Each layer has one job |
| **UI Decoupling** | ❌ Hardcoded keyboard only | ✅ Multiple UI methods |
| **Undo/Redo** | ❌ Not possible | ✅ Built-in via Command pattern |
| **Testability** | ❌ Callbacks hard to test | ✅ Commands are unit testable |
| **Extensibility** | ❌ Add UI = modify core | ✅ Add new UI without touching core |
| **Hot-Reload** | ❌ Not supported | ✅ File watcher + diff commands |
| **OSC/Remote** | ❌ Would require callbacks | ✅ Just create commands |

---

## Next Steps

1. **Start Phase 1** - Implement Graph::toJson() and Graph::fromJson()
2. **Test serialization** - Save/load round-trip with current graph
3. **Proceed to Phase 2** - Add Command system
4. **Iterate** - Each phase builds on the previous

**Estimated Total Time**: 12-18 hours across all phases

**Immediate Priority**: Phase 1 (Graph serialization) - This unblocks everything else and removes the broken PatchLoader completely.
