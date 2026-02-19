# Crumble

Minimal modular video graph system for openFrameworks.

## Features

- Dynamic multi-layer video blending (64+ layers)
- HAP codec for efficient video playback
- TouchDesigner-style node composition
- Pull-based evaluation for data flow

## Building

```bash
cd Crumble
make
```

## Usage

- Drag `.mov` or `.hap` files onto the window to add video layers
- `+/-` : Add/Remove layers
- `1-8` : Select & toggle layer
- `[/]` : Adjust opacity
- `B` : Cycle blend mode (ALPHA → ADD → MULTIPLY)
- `G` : Toggle GUI
- `T` : Add 10 test layers

## Architecture

```
src/
├── core/
│   ├── Node.h          # Base node class
│   ├── Graph.h/.cpp    # Graph IS a Node, manages connections
│   └── PatchLoader.h/.cpp  # JSON patch loading (WIP)
└── nodes/
    └── video/
        ├── VideoFileSource.h/.cpp  # HAP video player
        ├── VideoMixer.h/.cpp       # Multi-layer blender
        └── ScreenOutput.h/.cpp     # Display sink
```

## Blend Modes

- **ALPHA**: Standard alpha blending (layer 1)
- **ADD**: Additive blending (layers 2, 4, ...)
- **MULTIPLY**: Multiply blending (layers 3, 5, ...)
