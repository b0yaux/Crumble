# Speed Control Test Suite

## Overview

A comprehensive set of test scripts to verify that Crumble's video and audio playback correctly implement the runtime speed control feature.

## Test Scripts

### 1. `test_speed_control.lua` (2.3 KB)
**Purpose**: Main baseline test for speed control  
**Speed**: 1.0x (normal playback)  
**What it tests**:
- Basic video+audio loading and playback
- Speed parameter exists and is accessible
- Normal synchronization

**Use this first** to establish baseline playback works.

### 2. `test_speed_control_half.lua` (1.8 KB)
**Purpose**: Test slow-motion playback  
**Speed**: 0.5x (half speed)  
**What it tests**:
- Speed parameter can be set during initialization
- Video slows down correctly
- Audio slows down correctly
- A/V remains synchronized at reduced speed

**Run this to verify slow-motion works**.

### 3. `test_speed_control_double.lua` (1.9 KB)
**Purpose**: Test fast-forward playback  
**Speed**: 2.0x (double speed)  
**What it tests**:
- Speed parameter works at higher values
- Fast-forward maintains playback quality
- Audio pitch rises appropriately at 2x
- Potential A/V drift becomes apparent (if it exists)

**Run this to stress-test speed control** and detect sync issues.

## Test Data

All scripts use the same video and audio files from the superstratum_video-data directory:

- **Video**: `0-playing-around-with-a-CRT-projector_clip_00.mov`
  - Size: 301 MB
  - Codec: HAP (hardware-accelerated playback)
  - Resolution: ~1080p
  - Duration: ~30 seconds

- **Audio**: `0-playing-around-with-a-CRT-projector_clip_00.wav`
  - Size: 1.5 MB
  - Format: WAV, stereo, 44.1 kHz
  - Duration: Matches video

- **Location**: `/Users/jaufre/works/superstratum_video-data/`

These same files are used by `main.lua` and other production scripts, ensuring test conditions match real usage.

## How to Run

### Quick Start

1. **Pick a test script** (start with `test_speed_control.lua`)
2. **Edit `bin/data/config.json`**:
   ```json
   {
     "entry_script": "scripts/test_speed_control.lua"
   }
   ```
3. **Build and run**:
   ```bash
   make clean
   make
   make RunRelease
   ```

### Sequential Testing

Run tests in order to verify incrementally:

```bash
# Test 1: Normal speed (baseline)
# Edit config.json, set entry_script to test_speed_control.lua
make RunRelease
# Expected: Smooth playback, A/V in sync
# Duration: ~30 seconds of video

# Test 2: Half speed (slow-motion)  
# Edit config.json, set entry_script to test_speed_control_half.lua
make RunRelease
# Expected: Playback at 50% speed, still synchronized
# Duration: ~60 seconds (2x longer)

# Test 3: Double speed (fast-forward)
# Edit config.json, set entry_script to test_speed_control_double.lua  
make RunRelease
# Expected: Playback at 200% speed, may see A/V drift
# Duration: ~15 seconds
```

## What to Look For

### ✓ Success Indicators

- **Video loads without errors**
  - Console shows "Successfully loaded and playing"
  - Video appears in window
  - No crash or hang

- **Audio plays correctly**
  - Sound comes from speakers (not muted)
  - Audio level appropriate (gain is 0.5)
  - No crackling or artifacts

- **Speed parameter is applied**
  - 0.5x: Noticeably slower, audio pitch lower
  - 1.0x: Normal playback
  - 2.0x: Noticeably faster, audio pitch higher

- **A/V synchronization**
  - Video and audio stay together
  - No obvious lip-sync issues
  - At 1.0x speed: Should be perfectly synced

### ⚠ Warning Signs

| Issue | Cause | Action |
|-------|-------|--------|
| No audio output | File not found or loading failed | Check console for error messages; verify file path exists |
| Video stutters | Performance issue | Check CPU/GPU load; verify HAP codec supported |
| A/V drift at 2x speed | Different playback mechanisms | Expected; document as known limitation |
| Speed doesn't change | Parameter not connected | Check VideoFileSource::onParameterChanged() implementation |
| Reverse playback fails (if tested) | Speed parameter signed correctly | Check that negative speeds work |

### ✗ Failure Conditions

**Critical (should not happen)**:
- Application crashes
- Video black screen after loading
- Audio doesn't load at all
- Speed parameter doesn't exist (error about unknown parameter)

**High-priority bugs**:
- A/V sync issues at 1.0x speed
- Speed changes cause glitches/pops
- Reverse playback (negative speed) doesn't work

**Known limitations** (acceptable):
- A/V drift at non-1.0x speeds
- Pitch shift when speed changes (expected behavior)
- Occasional frame skips at 2x on slower machines

## Console Output

You should see output similar to:

```
=== Speed Control Test: ... ===
Loading video: /Users/jaufre/works/superstratum_video-data/0-playing-around-with-a-CRT-projector_clip_00.mov
Loading audio: /Users/jaufre/works/superstratum_video-data/0-playing-around-with-a-CRT-projector_clip_00.wav
=== Test Setup Complete ===
Speed: 1.0x (normal playback)

Expected: Everything plays at normal speed
Check: Video and audio are synchronized
```

Plus OpenFrameworks/Crumble logs:
```
[notice] VideoFileSource: Successfully loaded and playing: ...
[notice] SpeakersOutput: Sound stream started successfully.
```

## Test Procedure Checklist

### Before Testing
- [ ] Crumble builds successfully (`make`)
- [ ] Test data directory exists: `/Users/jaufre/works/superstratum_video-data/`
- [ ] Both .mov and .wav files present for test clip
- [ ] System has speakers or headphones connected for audio test
- [ ] No other audio playing (minimize interference)

### During Test
- [ ] Console shows no errors
- [ ] Video window opens
- [ ] Video is visible and not frozen
- [ ] Audio plays (listen carefully for speed/pitch)
- [ ] Let test run for full ~30 seconds (or 60s for 0.5x)
- [ ] Watch for glitches, stutters, artifacts

### After Test
- [ ] Note any observed differences in:
  - Playback smoothness
  - Audio quality
  - Lip-sync accuracy
  - CPU/GPU usage
- [ ] If A/V drift detected, measure approximately how much
- [ ] Document results

## Extending the Tests

### Add More Speed Variations

Create `test_speed_control_reverse.lua`:
```lua
-- Test negative speed (reverse playback)
videoSource.speed = -1.0
audioSource.speed = -1.0
```

Create `test_speed_control_quarter.lua`:
```lua
-- Test very slow speed
videoSource.speed = 0.25
audioSource.speed = 0.25
```

Create `test_speed_control_quad.lua`:
```lua
-- Test very fast speed
videoSource.speed = 4.0
audioSource.speed = 4.0
```

### Add Dynamic Speed Changes

Once Crumble supports update callbacks, add runtime speed modulation:

```lua
-- Pseudo-code for future update() callback
function update(dt)
    local time = getElapsedTime()
    local speed = 0.5 + 1.5 * sin(time)  -- Oscillate 0.5x to 2.0x
    videoSource.speed = speed
    audioSource.speed = speed
end
```

### Test with Different Files

All files in `/Users/jaufre/works/superstratum_video-data/` are compatible. Try:
- Different file sizes (test memory usage)
- Different durations (test looping behavior)
- Different audio formats if available (test codec handling)

## Known Limitations

### A/V Synchronization at Different Speeds

**Issue**: Video and audio use fundamentally different playback mechanisms:
- **Video**: HAP player with `player.setSpeed()` (C++ level)
- **Audio**: Playhead position tracking (C++ callback, Lua-driven)

**Impact**: 
- At 1.0x: Should be perfectly synchronized
- At other speeds: May drift over time
- Amount of drift depends on buffer sizes and thread timing

**Status**: Documented in `RECURSIVE_MIXER_ARCHITECTURE.md`

**Workaround**: If precise sync required at different speeds, implement:
- Shared playhead position tracking
- Synchronization protocol between audio/video engines
- Clock-based timing instead of speed multipliers

## Files

| File | Purpose |
|------|---------|
| `test_speed_control.lua` | Baseline test, 1.0x speed |
| `test_speed_control_half.lua` | Slow-motion test, 0.5x speed |
| `test_speed_control_double.lua` | Fast-forward test, 2.0x speed |
| `TEST_SPEED_CONTROL.md` | This documentation |
| `RECURSIVE_MIXER_ARCHITECTURE.md` | Architecture guide (reference) |
| `RECURSIVE_SPEED_FIX.md` | Implementation details (reference) |

## Troubleshooting

### Test won't run
- [ ] Check config.json entry_script path is correct
- [ ] Verify file paths in test script are absolute
- [ ] Check console for Lua syntax errors

### No video output
- [ ] Is window opening? Check behind other windows
- [ ] Is video file corrupted? Try another from same directory
- [ ] Check console for HAP loading errors

### No audio output
- [ ] Are speakers/headphones connected?
- [ ] Is system volume muted? Check OS audio settings
- [ ] Is audio muted in config? (`aOutput.volume = 0` disables audio)

### A/V completely out of sync
- [ ] Try 1.0x speed test first (baseline)
- [ ] Check if issue is reproducible
- [ ] Note exact audio/video offset distance
- [ ] File a bug with offset measurement

## Support

For issues or questions:

1. **Check existing docs**:
   - `RECURSIVE_SPEED_FIX.md` - Implementation details
   - `RECURSIVE_MIXER_ARCHITECTURE.md` - Architecture overview
   - `README.md` - General Crumble documentation

2. **Debug checklist**:
   - Does test script have correct file paths?
   - Do video/audio files exist and are readable?
   - Are parameters named correctly (lowercase)?
   - Does build compile without errors?

3. **Report findings**:
   - Which test script failed?
   - What was expected vs. observed?
   - Console error messages?
   - System specs (Mac, Linux, Windows)?

---

**Last updated**: February 27, 2026  
**Crumble version**: 0.12.1  
**Test data**: superstratum_video-data collection
