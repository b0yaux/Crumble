# Codebase Structure

**Analysis Date:** 2026-02-21

## Directory Layout

```
[project-root]/
├── src/                # C++ Source Code
│   ├── core/           # Engine logic (Graph, Node, Session)
│   ├── nodes/          # Concrete node implementations
│   │   ├── audio/      # Audio source, effects, and output nodes
│   │   ├── composite/  # High-level macro nodes (nested graphs)
│   │   └── video/      # Video source, mixers, and output nodes
│   ├── ui/             # Graphical User Interface (Graph viz)
│   ├── crumble/        # Application-specific bootstrapping
│   ├── main.cpp        # C++ Entry point
│   └── ofApp.h/cpp     # Main openFrameworks App class
├── bin/                # Compiled binaries and runtime assets
│   └── data/           # Assets, scripts, and patches
│       ├── patches/    # JSON graph save files
│       ├── scripts/    # Lua scripts for graph generation
│       └── shaders/    # GLSL shaders (not detected but standard location)
├── docs/               # Architecture research and proposals
└── node_modules/       # Tooling and scripts (via npm)
```

## Directory Purposes

**src/core/:**
- Purpose: The "Brain" of the application. Handles graph topology, node lifecycle, and data flow.
- Contains: Base classes and orchestrators.
- Key files: `Node.h`, `Graph.h`, `Session.h`.

**src/nodes/:**
- Purpose: Domain-specific logic. Each file typically contains one or more `Node` subclasses.
- Contains: Audio and Video processing units.
- Key files: `VideoMixer.cpp`, `VideoFileSource.cpp`, `AudioMixer.h`.

**src/ui/:**
- Purpose: Visualization of the graph. Translates C++ graph state to screen coordinates.
- Contains: Drawing logic and mouse interaction for nodes.
- Key files: `GraphUI.cpp`.

**bin/data/:**
- Purpose: External assets and configuration.
- Contains: Lua scripts, JSON patches, media files.
- Key files: `scripts/main.lua`, `patches/default.json`.

## Key File Locations

**Entry Points:**
- `src/main.cpp`: C++ process entry.
- `src/ofApp.cpp`: High-level application setup and loop.
- `bin/data/scripts/main.lua`: Scripting-based entry for graph construction.

**Configuration:**
- `package.json`: NPM-based tooling configuration.
- `Project.xcconfig`: Xcode build settings.
- `addons.make`: openFrameworks addon list.

**Core Logic:**
- `src/core/Session.cpp`: The primary interface for interacting with the engine.
- `src/core/Graph.cpp`: Implementation of node connectivity and evaluation.

**Testing:**
- Not detected: No dedicated test directory found. Patterns suggest manual testing via `ofApp` and Lua scripts.

## Naming Conventions

**Files:**
- PascalCase: `VideoFileSource.cpp`, `GraphUI.cpp`.
- snake_case: (Lower-case files like `main.cpp`, `ofApp.cpp` follow openFrameworks conventions).

**Directories:**
- lower_case: `nodes`, `video`, `audio`, `core`.

## Where to Add New Code

**New Feature:**
- If it's a new engine capability: `src/core/`.
- If it's a new node type: `src/nodes/[category]/`.
- Don't forget to register it in `src/crumble/Registry.cpp`.

**New Component/Module:**
- Add a new folder in `src/nodes/` if it introduces a new category (e.g., `src/nodes/midi/`).

**Utilities:**
- Helper functions should go into `src/core/` if engine-related, or a new `src/utils/` if generic.

## Special Directories

**obj/:**
- Purpose: Build artifacts (compiler outputs).
- Generated: Yes
- Committed: No

**lost/:**
- Purpose: Likely a quarantine or recovery directory for orphaned files.
- Generated: No
- Committed: Yes

---

*Structure analysis: 2026-02-21*
