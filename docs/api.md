# Crumble API Reference

## Lua Scripting DSL

The Lua environment in Crumble provides a functional, reactive API for graph composition and live-coding.

### Graph & Node Management

- `addNode(type, name)`: Creates or updates a node of the specified `type`. `name` is optional but recommended for idempotent script reloading. Returns a proxy table for the node.
- `connect(source, destination, sourceOutputIndex, destInputIndex)`: Connects two nodes. Can also accept an array of sources (e.g., `connect({n1, n2}, mixer)`), or glob patterns (e.g., `connect("smp*", mixer)`).
- `clear()`: Deletes all nodes and connections in the current graph context.

### Node Parameters & Modulators

Node parameters can be read or written directly via property access:
```lua
node.volume = 0.5
print(node.volume)
```

Modulators (C++ Generators) can be assigned seamlessly using the math DSL:
```lua
-- Sequences
node.speed = seq("1 0.5 0.25")

-- LFOs
node.opacity_0 = osc(2.0)  -- Sine wave at 2Hz
node.opacity_1 = ramp(1.0) -- Sawtooth wave at 1Hz

-- Combinations (Operator Overloading)
node.volume = seq("1 0 1 0") * 0.5
node.blend_0 = osc(0.1) + seq("0 0.5")
```

### Context Global Functions

- `importFolder(path, [extensions])`: Imports a directory of media files and creates `VideoFileSource` or `AudioFileSource` nodes automatically.
- `fileExists(path)`: Returns true if the path exists.
- `_listDir(path)`: Returns an array of paths in the given directory.

### Event Hooks

- `update(Time)`: If declared in the root script, called every UI frame. The `Time` table contains:
  - `Time.absoluteTime`: Total seconds since start.
  - `Time.cycle`: Current beat phase (0.0 to 1.0).
  - `Time.bpm`: Global tempo.
  - `Time.isPlaying`: Transport state.

---

## C++ Plugin API

To create a new Node in C++, inherit from `Node` and register it in `Session`.

### Creating a Audio Node
```cpp
class MyAudioNode : public Node {
public:
    ofParameter<float> frequency;

    MyAudioNode() {
        type = "MyAudioNode";
        parameters.add(frequency.set("freq", 440.0, 20.0, 20000.0));
    }

    void pullAudio(ofSoundBuffer& buffer, int index = 0) override {
        // Retrieve block-vectorized signal 
        Signal freqSig = getSignal(frequency);

        for(size_t i = 0; i < buffer.getNumFrames(); i++) {
            float f = freqSig[i]; // Sample-accurate modulation!
            // ... process audio ...
        }
    }
};
```

### Creating a Video Node
```cpp
class MyVideoNode : public Node {
public:
    ofParameter<float> radius;
    ofFbo fbo;

    MyVideoNode() {
        type = "MyVideoNode";
        canDraw = true; // Participate in UI draw loop
        parameters.add(radius.set("radius", 100.0, 0.0, 500.0));
        fbo.allocate(1920, 1080);
    }

    void update(float dt) override {
        // Video nodes evaluate dynamically for the current frame
        Signal radSig = getSignal(radius);
        float currentRadius = radSig[0];
        
        fbo.begin();
        ofClear(0);
        ofDrawCircle(fbo.getWidth()/2, fbo.getHeight()/2, currentRadius);
        fbo.end();
    }

    ofTexture* getVideoOutput(int index = 0) override {
        return &fbo.getTexture();
    }
};
```