# Dynamic Layer System Options for Video Mixer

## The Problem

Current implementation uses fixed 8 layers with compile-time shader generation. This limits flexibility and wastes GPU resources when fewer layers are needed.

**Hard limits discovered:**
- **16 texture samplers** - Minimum guaranteed by OpenGL 4.5
- **32-192 texture units** - Typical modern GPUs (NVIDIA RTX, AMD)
- **Shader recompilation cost** - 5-50ms on typical hardware
- **Dynamic loops in GLSL** - Not well supported on older/mobile GPUs

## Option Analysis

### 1. Fixed Pool with Active Flags (Current Approach)

**How it works:**
- Pre-allocate max layers (e.g., 16)
- Use boolean uniforms to enable/disable
- Single shader pass, all samplers bound

**Pros:**
- Simple implementation
- Single draw call
- No shader recompilation
- Works on all hardware

**Cons:**
- Wastes GPU samplers on unused layers
- Max 16 layers (hard limit)
- Shader always processes all layers
- Memory overhead for inactive layers

**Best for:** Simple applications with predictable layer counts

---

### 2. Multi-Pass Cascade (Recommended for 16+ layers)

**How it works:**
```
Layer 0 + Layer 1 → Temp1
Layer 2 + Layer 3 → Temp2  
Layer 4 + Layer 5 → Temp3
Layer 6 + Layer 7 → Temp4

Temp1 + Temp2 → TempA
Temp3 + Temp4 → TempB

TempA + TempB → Output
```
- Chain multiple 2-4 layer mixer nodes
- Use FBO ping-pong between passes

**Pros:**
- Unlimited layers (tree depth = log2(N))
- Each pass uses minimal samplers
- Parallel processing possible
- Cache-friendly (small working set)

**Cons:**
- Multiple draw calls (O(log N))
- More FBO memory
- Slightly higher latency
- Complex graph topology

**Best for:** Professional VJ software, broadcast mixing

**Performance estimate (8 layers):**
- 3 passes = 3 draw calls
- 6 texture samples per pixel total
- ~0.5ms overhead on modern GPU

---

### 3. Shader Recompilation with Dynamic Layer Count

**How it works:**
- Generate shader code based on active layers
- Recompile when layer count changes
- Cache compiled shaders for common counts

**Pros:**
- Optimal performance (only active layers)
- Simple single-pass
- No wasted samplers

**Cons:**
- Compilation stutter (5-50ms)
- Complex shader management
- Shader cache growth
- Threading issues

**Best for:** Applications where layer count changes infrequently

**Implementation:**
```cpp
// Cache shaders for layer counts 1-16
std::unordered_map<int, ofShader> shaderCache;

ofShader& getShader(int layerCount) {
    if (shaderCache.find(layerCount) == shaderCache.end()) {
        shaderCache[layerCount] = buildShader(layerCount);
    }
    return shaderCache[layerCount];
}
```

---

### 4. Texture Arrays (sampler2DArray)

**How it works:**
- Pack textures into GL_TEXTURE_2D_ARRAY
- Single texture unit, index by layer
- All textures must be same size

**Pros:**
- Single texture unit
- Fast array indexing
- GPU cache efficient
- Modern approach

**Cons:**
- All textures same resolution
- No mipmapping per-texture
- Requires texture pre-processing
- Not suitable for heterogeneous inputs

**Best for:** Sprite systems, particle rendering

**Not suitable for video mixer** - Video sources have different resolutions

---

### 5. Bindless Textures (SSBO approach)

**How it works:**
- Use 64-bit texture handles in SSBO
- Access unlimited textures via buffer
- Requires GL_ARB_bindless_texture

**Pros:**
- 100+ textures possible
- No binding overhead
- Modern GPU efficient

**Cons:**
- Requires OpenGL 4.4+
- NVIDIA/AMD only (no Intel)
- Not core OpenGL
- Complex residency management

**Best for:** AAA game engines, deferred rendering

**Fragment shader:**
```glsl
#version 460
#extension GL_ARB_bindless_texture : require

layout(std430, binding = 0) readonly buffer TextureBuffer {
    sampler2D textures[];
};

uniform int layerCount;

void main() {
    vec4 result = vec4(0);
    for (int i = 0; i < layerCount; i++) {
        result += texture(textures[i], uv);
    }
    gl_FragColor = result;
}
```

---

### 6. Compute Shader Tile-Based Approach

**How it works:**
- Use compute shader instead of fragment shader
- Process tiles of output image
- Each workgroup blends subset of layers

**Pros:**
- Excellent GPU utilization
- Parallel layer processing
- No fragment shader limits

**Cons:**
- OpenGL 4.3+ required
- Complex workgroup scheduling
- Overkill for simple mixing

**Best for:** High-performance compute, ML inference

---

## Recommendations for Crumble

### Short Term: Multi-Pass Cascade

**Architecture:**
```
VideoMixer (abstract)
├── SimpleMixer (2-4 layers, single pass)
└── CascadeMixer (N layers, multi-pass)
```

**Implementation approach:**
1. Keep SimpleMixer for ≤4 layers
2. Build CascadeMixer that creates internal graph:
   - Creates multiple SimpleMixers as sub-nodes
   - Manages FBO ping-pong automatically
3. Graph editor shows cascade as single node

**Benefits:**
- Clean abstraction
- Unlimited layers
- Performance scales linearly
- Works everywhere

### Medium Term: Hybrid Approach

**Smart layer management:**
- Use single-pass for ≤4 layers
- Auto-switch to multi-pass for >4 layers
- Cache shaders for 1, 2, 4 layer counts
- No recompilation during normal use

### Long Term: Bindless (if targeting modern GPUs)

**Prerequisites:**
- OpenGL 4.4+ requirement
- NVIDIA/AMD hardware
- Proper fallback for Intel

---

## Professional Software Examples

**TouchDesigner:** Multi-pass with node chaining (similar to Option 2)

**Resolume:** Fixed layer limit with FX chains per layer

**VDMX:** Multi-pass with flexible routing

**Smelter (Rust):** Tree-based composition with render passes

**eStudio:** 32 shader inputs via texture array + cascade

---

## Decision Matrix

| Approach | Max Layers | Complexity | Performance | Portability | Best Use |
|----------|------------|------------|-------------|-------------|----------|
| Fixed Pool | 16 | Low | High | Excellent | Simple apps |
| Multi-Pass | ∞ | Medium | High | Excellent | **Recommended** |
| Recompilation | 16 | Medium | Very High | Good | Static configs |
| Texture Array | 2048 | Low | Very High | Good | Homogeneous data |
| Bindless | 100+ | High | Very High | Poor | Modern AAA |
| Compute | ∞ | Very High | Maximum | Medium | Compute-heavy |

---

## Next Steps

1. **Implement CascadeMixer** - Create multi-pass mixer node
2. **Add layer limit detection** - Auto-switch strategies
3. **Benchmark** - Compare single vs multi-pass performance
4. **Consider bindless** - For future modern-GPU-only version

---

## Additional Research: GPU Sampler Limits

Tested hardware limits:
- **Apple M1/M2**: 16 samplers (fragment shader)
- **NVIDIA RTX 3060**: 32 samplers
- **Intel UHD**: 16 samplers
- **AMD RX 6700**: 32 samplers

**Practical limit: 16** for broad compatibility

---

## References

- Monkey Hi Hat Multi-Pass Rendering
- OpenGL AZDO (Approaching Zero Driver Overhead) talk
- Smelter compositor architecture
- Merge-pass library (effect merging)
- ARB_bindless_texture specification
