# A/V Synchronization Analysis: Current Architecture & Drift Problem

## Current Situation: You've Confirmed a Real Problem

Your tests have revealed a **fundamental architectural issue**:

✓ **What works**: Individual speed control for video and audio sources  
✗ **What doesn't work**: A/V synchronization when sources have different speeds or when speed changes

## Root Cause: Two Completely Independent Playback Engines

### Video Playback (HAP Player)
```cpp
// VideoFileSource.cpp:64-67
void VideoFileSource::onParameterChanged(const std::string& paramName) {
    if (paramName == "speed" && player.isLoaded()) {
        player.setSpeed(speed);  // Direct HAP player speed control
    }
}
```

**Characteristics**:
- Uses external HAP decoder library (`ofxHapPlayer`)
- Speed is set via `player.setSpeed(speedValue)`
- HAP player controls its own internal frame timing
- Runs **asynchronously** in separate thread/buffer
- Updates in `VideoFileSource::update(float dt)` → `player.update()`

### Audio Playback (Lua-Driven Playhead)
```cpp
// AudioFileSource.cpp:26-37
for (size_t i = 0; i < buffer.getNumFrames(); i++) {
    size_t frameIndex = (size_t)playhead;
    
    // ... read sample at frameIndex ...
    
    playhead += (float)speed;  // Manual playhead advancement
    
    // ... handle looping ...
}
```

**Characteristics**:
- Stores audio in RAM as float buffer (`ofxAudioFile`)
- Playhead position is tracked manually in C++
- Speed is applied **per-sample** in the audio callback
- Runs **synchronously** when `SpeakersOutput::audioOut()` is called
- Timing driven by OS audio system (typically 44.1kHz or 48kHz)

## Why They Drift

### The Timing Mismatch

| Aspect | Video (HAP) | Audio (Playhead) |
|--------|-----------|-----------------|
| **Timing source** | HAP player's internal clock | OS audio system (fixed sample rate) |
| **Update frequency** | Frame-based (depends on video FPS) | Sample-based (44.1k or 48k samples/sec) |
| **Speed response** | Applied by HAP codec | Applied sample-by-sample |
| **Drift mechanism** | Frame timing jitter + HAP buffering | Depends on buffer callback timing |

### Concrete Example

Suppose both start at the same time with `speed = 1.0`:

**Frame 0 (time = 0ms)**
- Video: Frame 0 displayed
- Audio: Sample 0 played

**After 100ms at speed = 1.0x**
- Video: ~3 frames later (at 30fps) = frame 3 displayed
- Audio: 4,410 samples later (at 44.1kHz) = sample 4,410

**They change speed to 0.5x**

**Video**: HAP player sees `setSpeed(0.5)` and starts playing at half speed
- Next frame could be: frame 3.5 (or 4, depending on HAP's internal buffer state)

**Audio**: Playhead sees `speed = 0.5` and starts advancing by 0.5 samples per iteration
- Next sample could be: 4,410.5 (or whatever was next in the callback)

**The Problem**: 
- Video's speed change takes effect when the HAP decoder gets the next frame
- Audio's speed change takes effect on the next audio buffer callback
- These don't happen at the same time → **drift begins**
- Drift **accumulates** because they're updating at different rates

## Why This Is Hard to Fix

### Option 1: Shared Master Clock (Complex)

Need a **unified timing system** that both players respect:

```cpp
class SharedPlayhead {
    double masterPosition;  // in samples or frames
    float speed;
    
    // Video: "where should I be?"
    double getVideoFrame() { return masterPosition / samplesPerFrame; }
    
    // Audio: "where should I be?"
    double getAudioSample() { return masterPosition; }
};
```

**Problems**:
- Video FPS ≠ Audio sample rate (incommensurable)
- Need to convert between frame space and sample space
- HAP decoder is a black box (can't easily force it to a specific frame)
- Audio callback timing is OS-controlled (can't guarantee sync)

### Option 2: Resync on Speed Change (Partial Fix)

When speed changes, reset both to same position:

```cpp
void AudioMixer::onSpeedChanged() {
    // Get current video position
    int videoFrame = videoSource->getCurrentFrame();
    
    // Convert to audio samples
    double audioSample = (videoFrame * audioSampleRate) / videoFPS;
    
    // Seek audio to that position
    audioSource->setPlayhead(audioSample);
}
```

**Problems**:
- Only fixes drift at the moment of change
- Drift continues accumulating after change
- Jumpy user experience (audio skips when speed changes)
- Complex coordinate conversion between video/audio domains

### Option 3: Accept Drift + Offer Resync (Pragmatic)

Document it as a limitation and provide manual resync:

```lua
-- User can manually resync if drift becomes noticeable
syncAudioToVideo(videoSource, audioSource)
```

**Advantages**:
- Minimal code changes
- Honest about limitations
- User has control
- Good for short clips or live performance

**Disadvantages**:
- Not automatic
- Requires user intervention
- Bad for long playback sessions

## The Real Question: What's Crumble's Use Case?

This fundamentally changes which solution makes sense:

### If Crumble is for **Live Performance** (< 5 minutes)
- Accept drift as known limitation
- Resync manually between clips
- Drift probably won't be noticeable in short clips

### If Crumble is for **Long-Form Playback** (> 5 minutes)
- Must implement unified timing
- Drift will accumulate, users will notice
- Requires architectural redesign

### If Crumble is for **Generative/Speed Modulation** (frequent changes)
- Must prevent drift on every speed change
- Requires shared playhead + resync protocol
- Most expensive solution

## Recommended Path Forward

Based on your confirmed testing, here's my advice:

### Phase 1: Document & Acknowledge (Low effort, high value)
- [ ] Update documentation to clearly state: **"Separate audio and video sources drift when played at non-1.0x speed"**
- [ ] Add to `RECURSIVE_MIXER_ARCHITECTURE.md` why this happens
- [ ] Provide `resyncAudioToVideo()` Lua helper function for manual fixes

### Phase 2: Measure the Problem (Medium effort, reveals scope)
- [ ] Quantify drift: How much deviation after 30s at 0.5x? After 1m at 2.0x?
- [ ] Document acceptable thresholds (e.g., "< 50ms drift is acceptable")
- [ ] Test with different audio sample rates (44.1k vs 48k)

### Phase 3: Choose Solution Based on Data

**If drift is < 50ms even after 2+ minutes**:
- Drift is probably acceptable for most use cases
- Stop here. Document it and move on.
- Focus on other features instead.

**If drift is > 50ms or > 100ms**:
- Implement Option 3 (manual resync API)
- Add Lua functions: `getAudioPosition()`, `setAudioPosition()`, `syncAudioToVideo()`
- Let users control sync strategically

**If drift is > 200ms or increases rapidly**:
- Implement Option 1 (shared master clock)
- This is a **major refactor** — would affect:
  - How `update(dt)` works
  - How speed parameter is stored (global vs per-node)
  - How playback state is tracked
- Only do this if drift is unacceptable

## Honest Assessment

**Don't add new features until you understand this.** Here's why:

1. **This affects everything audio/video related**: speed control, looping, seeking, recording
2. **Architecture decision now = easier refactor later**: If you build features on top of broken A/V sync, fixing it will be painful
3. **You've discovered a real limitation**: That's valuable. Document it, then proceed intelligently

## My Recommendation

**Start with Phase 1 + 2** (documentation + measurement):

1. **This week**: Document the drift issue clearly
2. **This week**: Write a test that measures drift quantitatively
   ```cpp
   // Pseudo-code
   video.speed = 0.5;
   audio.speed = 0.5;
   sleep(30 seconds);
   driftMs = video.position - audio.position;
   assert(driftMs < 50, "Unacceptable drift");
   ```
3. **Measure actual numbers**: How bad is it really?
4. **Make a conscious decision**: Is it acceptable or not?

Only after you have those numbers should you decide whether to:
- Accept it as a known limitation (Option 3)
- Build a manual resync API (Option 3+)
- Implement unified timing (Option 1 — big refactor)

**Then proceed to add new features** with eyes open about the constraints.

---

## Questions for You

1. **What's the typical use case?** Live performance? Generative art? Long playback sessions?
2. **Do users typically change speed during playback?** Or is speed set at startup?
3. **How important is perfect A/V sync?** Acceptable tolerance?
4. **How long are typical playback sessions?** (Drift accumulates over time)

Your answers determine whether this is "document and move on" or "invest in architecture redesign."
