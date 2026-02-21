# Technology Stack

**Analysis Date:** 2026-02-21

## Languages

**Primary:**
- C++ 23 - Core application logic, node graph engine, and rendering. Used throughout `src/`.

**Secondary:**
- Lua 5.1 (via ofxLua) - User scripting for dynamic graph generation and automation. Located in `bin/data/scripts/`.

## Runtime

**Environment:**
- openFrameworks v0.12.1 - Creative coding framework.
- macOS 11.5+ - Targeted platform (based on `Project.xcconfig`).

**Package Manager:**
- openFrameworks Addons - Managed via `addons.make`.
- npm (partial) - `package.json` present but only contains `nvm`.

## Frameworks

**Core:**
- openFrameworks - Application framework and window management.

**Testing:**
- Not detected - No explicit testing framework found.

**Build/Dev:**
- Xcode 15+ - Primary build system via `Crumble.xcodeproj`.
- Make - Secondary build system via `Makefile` and `config.make`.

## Key Dependencies

**Critical:**
- `ofxLua` - Provides Lua scripting capabilities for live-coding the node graph.
- `ofxHapPlayer` - Enables high-performance Hap video codec playback.
- `ofxAudioFile` - Handles audio file loading and decoding.

**Infrastructure:**
- `nlohmann::json` (via `ofJson`) - Used for session persistence and undo/redo state snapshots.

## Configuration

**Environment:**
- Not applicable - No `.env` files or environment-based configuration detected.

**Build:**
- `Project.xcconfig` - Xcode build settings (C++ standard, deployment target).
- `config.make` - Makefile configuration.
- `addons.make` - List of openFrameworks addons to link.

## Platform Requirements

**Development:**
- macOS with Xcode installed.
- openFrameworks v0.12.1 SDK in the parent directory path.

**Production:**
- macOS (Universal or x86_64/arm64 depending on build).

---

*Stack analysis: 2026-02-21*
