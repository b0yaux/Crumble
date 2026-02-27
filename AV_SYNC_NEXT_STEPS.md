# A/V Sync: What to Do Next

## Summary of the Situation

You've discovered something important: **Video and audio sources drift when played at different speeds or when speeds change during playback.**

This is not a bug in the speed control implementation — it's a **fundamental architectural limitation** of having two completely independent playback engines:
- Video: HAP decoder (external library, frame-based, asynchronous)
- Audio: Manual playhead tracking (C++, sample-based, synchronous)

## Key Decision Point

**Before adding more features, decide: Is this acceptable for Crumble's use case?**

The answer depends on three factors:

### 1. How Long Are Your Playback Sessions?
- **< 30 seconds**: Drift probably unnoticeable
- **30 seconds - 5 minutes**: Drift noticeable but maybe acceptable
- **> 5 minutes**: Drift becomes a serious problem

### 2. Do Users Change Speed During Playback?
- **No**: Speed set once at startup → drift doesn't matter much
- **Occasionally**: Resync could be acceptable  
- **Frequently**: Drift intolerable, architecture redesign needed

### 3. How Critical Is Perfect A/V Sync?
- **Casual use** (experimental, generative): A few frames drift is fine
- **Performance** (live art): Noticeable drift is bad
- **Media playback** (film, music video): Must be frame-perfect

## Recommended Next Steps

### Step 1: Measure the Actual Drift (This Week)

Write a test that quantifies drift in real conditions:

```lua
-- test_av_drift_measurement.lua
-- Measure how much audio/video drift over time at different speeds

local videoSource = addNode("VideoFileSource", "vid")
videoSource.path = "/Users/jaufre/works/superstratum_video-data/0-playing-around-with-a-CRT-projector_clip_00.mov"

local audioSource = addNode("AudioFileSource", "aud")
audioSource.path = "/Users/jaufre/works/superstratum_video-data/0-playing-around-with-a-CRT-projector_clip_00.wav"

-- Connect to outputs
local videoOutput = addNode("ScreenOutput", "screen")
local audioOutput = addNode("SpeakersOutput", "speakers")
local videoMixer = addNode("VideoMixer", "vmixer")
local audioMixer = addNode("AudioMixer", "amixer")

connect(videoSource, videoMixer, 0, 0)
connect(audioSource, audioMixer, 0, 0)
connect(videoMixer, videoOutput)
connect(audioMixer, audioOutput)

-- TEST CASES
-- Test A: 0.5x for 60 seconds
videoSource.speed = 0.5
audioSource.speed = 0.5

-- Test B: 2.0x for 30 seconds  
-- videoSource.speed = 2.0
-- audioSource.speed = 2.0

-- Test C: Change speed after 15 seconds
-- At 15s: set both to 2.0x
-- At 30s: set both to 0.5x

print("TEST: A/V Drift Measurement")
print("Speed: " .. videoSource.speed .. "x")
print("Watch for: Do audio and video sound/look synchronized?")
print("Listen for: Does audio stay locked to video, or drift?")
print("")
print("How to measure:")
print("1. Find a distinctive audio/visual event (beat, word, etc.)")
print("2. Listen/watch when that event occurs")
print("3. Notice if audio lags or leads video")
print("4. Estimate offset in milliseconds")
print("")
```

**Manual process** (since Crumble doesn't have frame-by-frame query yet):
1. Run the test
2. Watch/listen carefully for 30-60 seconds
3. Look for visual moments (cuts, flashes) that have audio cues (beats, words)
4. Notice if audio and video are in sync or not
5. Estimate: "audio lags video by ~100ms" or "they're perfectly in sync"

**Better approach** (if you implement it): Add debugging functions

```cpp
// In VideoFileSource header
int getCurrentFrame() const;

// In AudioFileSource header  
int getCurrentSample() const;

// In ScriptBridge, expose to Lua
lua.setFunction("getVideoFrame", [this]() { 
    return videoSource->getCurrentFrame(); 
});
lua.setFunction("getAudioSample", [this]() { 
    return audioSource->getCurrentSample(); 
});
```

Then write a Lua script that logs these periodically:
```lua
function checkSync()
    local vFrame = getVideoFrame()
    local aSample = getAudioSample()
    local sampleRate = 44100
    local fps = 30
    
    local vSample = (vFrame * sampleRate) / fps
    local drift = aSample - vSample
    local driftMs = (drift / sampleRate) * 1000
    
    print("Drift: " .. driftMs .. "ms (audio " .. 
          (drift > 0 and "ahead" or "behind") .. ")")
end
```

### Step 2: Decide on Acceptable Tolerance

Based on your measurements, define thresholds:

```
If drift < 20ms:  "Unnoticeable, not a problem"
If drift 20-50ms: "Barely noticeable, acceptable for most uses"
If drift 50-100ms: "Noticeable, requires user attention"
If drift > 100ms: "Unacceptable, must fix"
```

### Step 3: Choose Your Solution

Based on drift measurements + use case:

#### Option A: Accept & Document (If drift < 50ms)
- **Effort**: 30 minutes
- **Action**: 
  1. Update documentation: "A/V sources drift at non-1.0x speeds"
  2. Add note to TEST_SPEED_CONTROL_SUITE.md about known limitation
  3. Suggest: "Use 1.0x speed for synchronized playback"
- **Result**: Move forward, build other features, revisit if it becomes a problem

#### Option B: Manual Resync API (If drift 50-100ms)
- **Effort**: 2-4 hours
- **Action**:
  1. Add helper functions in AudioFileSource:
     ```cpp
     void seek(int sampleIndex);
     int getCurrentSample() const;
     ```
  2. Add to ScriptBridge:
     ```lua
     function syncAudioToVideo(audioNode, videoNode)
         local vFrames = videoNode.currentFrame
         local aSamples = (vFrames * 44100) / 30
         audioNode.currentSample = aSamples
     end
     ```
  3. Document: "Call syncAudioToVideo() if drift becomes noticeable"
- **Result**: Users can fix drift manually, good for short clips

#### Option C: Shared Master Clock (If drift > 100ms)
- **Effort**: 3-5 days refactor
- **Action**: 
  1. Create `PlaybackController` class that owns timing
  2. Both video and audio read from shared clock
  3. Update graph update cycle to pull from master clock
- **Result**: Solved, but touches core architecture

## Honest Recommendation

**Based on typical Crumble use cases, I recommend Option B (Manual Resync):**

Here's why:

1. **Crumble is not a media player** — it's a live/generative system
2. **Most use cases won't hit the problem** — either:
   - Speed stays at 1.0x (no drift)
   - Playback is short (drift doesn't accumulate)
   - Speed changes are deliberate artistic decisions, not "oops I need to fix sync"
3. **Manual control is honest** — users know when they change speed, they can resync if needed
4. **Low implementation cost** — just adds seeking functions
5. **Doesn't block other features** — you can add this incrementally

## What NOT to Do

❌ **Don't build new features on top of broken A/V sync**
- Each new feature (recording, pitch shifting, loop points) will be harder to fix later
- Better to understand limitations now

❌ **Don't try to fix with hacky workarounds**
- Trying to "fight" the timing differences will be fragile
- Accept the architecture as-is, document it, work around it

❌ **Don't refactor the entire timing system yet**
- Wait until you know it's actually a problem
- It might not matter for your use case

## Timeline Suggestion

**Week 1: Measure & Understand**
- Run drift measurement test
- Document findings: "X ms drift after Y seconds at Z speed"
- Write one clear sentence: "Is this acceptable for Crumble's use case?"

**Week 2: Implement Phase 1 or 2**
- If acceptable: Document limitation, move on
- If not: Add manual resync API

**Week 3+: Add other features**
- Now you've dealt with the A/V sync issue responsibly
- Can build confidently knowing constraints

## Questions to Answer Before Proceeding

1. **What's Crumble being used for?** (Live performance? Generative art? Media playback?)
2. **How long are typical sessions?** (seconds? minutes?)
3. **Do users expect perfect sync or is artistic drift ok?**
4. **Will speed control be used frequently or just for special effects?**

Once you answer these, the right solution becomes obvious.

---

## For Right Now: Keep Testing

The test suite you have is great. Run `test_speed_control.lua` variants and:
- Note whether audio/video drift is perceptible
- Time how long until drift becomes > 100ms
- Test with different speeds (0.25x, 0.5x, 1.0x, 2.0x, 4.0x)
- Test changing speeds mid-playback

Document observations. That data will drive the next decision.

**You've done good empirical work. Now use the data to decide.**
