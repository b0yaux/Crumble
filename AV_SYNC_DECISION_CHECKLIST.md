# A/V Sync: Quick Decision Checklist

Use this to decide which path to take.

## Part 1: Measure the Problem (15 minutes)

Run these tests and listen/watch carefully:

```bash
# Edit bin/data/config.json
{ "entry_script": "scripts/test_speed_control_half.lua" }

make RunRelease
# Let it play for 30-60 seconds
# Listen: Can you hear audio drifting from video?
# Note: [unnoticeable / barely noticeable / noticeable / very noticeable]
```

Repeat with:
- `test_speed_control.lua` (1.0x baseline)
- `test_speed_control_double.lua` (2.0x fast)

## Part 2: Answer These Questions

### Session Duration
```
How long are typical Crumble sessions?
  □ < 30 seconds
  □ 30 seconds - 2 minutes
  □ 2-5 minutes
  □ > 5 minutes
```

### Speed Changes
```
How often do you change speed during playback?
  □ Never (set at start)
  □ Rarely (occasional artistic changes)
  □ Often (frequent modulation)
  □ Very often (every few seconds)
```

### Sync Criticality
```
How important is perfect A/V sync?
  □ Not important (generative/experimental)
  □ Somewhat important (should be noticeable)
  □ Important (users will notice problems)
  □ Critical (frame-perfect required)
```

## Part 3: Choose Your Path

Based on your answers:

### IF: Drift is unnoticeable + session < 2 min + casual use
→ **CHOOSE OPTION A** (Accept & Document)

**Why**: The problem doesn't actually exist for your use case

**Action items**:
- [ ] Add note to README: "A/V sources may drift at non-1.0x speeds"
- [ ] Update TEST_SPEED_CONTROL_SUITE.md with this limitation
- [ ] Move on to other features

**Effort**: 30 minutes

---

### IF: Drift is noticeable + session 2-5 min + occasional changes
→ **CHOOSE OPTION B** (Manual Resync API)

**Why**: Drift matters but users can handle occasional resyncing

**Action items**:
- [ ] Add `getCurrentSample()` to AudioFileSource
- [ ] Add `seek(sampleIndex)` to AudioFileSource
- [ ] Write Lua helper function `syncAudioToVideo()`
- [ ] Add example to TEST_SPEED_CONTROL_SUITE.md
- [ ] Document in README

**Example API**:
```lua
-- Helper function
function syncAudioToVideo(audioNode, videoNode)
    -- Get video position in seconds
    local videoPosMs = videoNode.currentFrameMs
    
    -- Convert to audio samples (assuming 44.1kHz)
    local audioSample = (videoPosMs / 1000) * 44100
    
    -- Seek audio to that position
    audioNode.seek(audioSample)
    
    print("Synced audio to video")
end

-- Usage
syncAudioToVideo(audioSource, videoSource)
```

**Effort**: 2-4 hours

---

### IF: Drift is noticeable + session > 5 min + frequent changes
→ **CHOOSE OPTION C** (Unified Timing Architecture)

**Why**: Drift is unacceptable for your use case, must be fixed

**Action items**:
- [ ] Design shared playback controller
- [ ] Refactor update loop to use master clock
- [ ] Update both AudioFileSource and VideoFileSource
- [ ] Extensive testing at various speeds
- [ ] Document changes

**Effort**: 3-5 days refactor

---

## Part 4: Document Your Decision

Create a new file: `AV_SYNC_DECISION.md`

```markdown
# A/V Synchronization Decision

**Date**: [Today]
**Decision**: OPTION [A/B/C]
**Rationale**: [Why this option for Crumble's use case]

## Measurements
- Drift at 1.0x: [unnoticeable/noticeable/etc]
- Drift at 0.5x: [measurement]
- Drift at 2.0x: [measurement]

## Use Case
- Typical session: [length]
- Speed changes: [frequency]
- Sync criticality: [importance level]

## Timeline
Implementation will be completed by: [Date]

## Next Steps
1. [Action 1]
2. [Action 2]
3. [Action 3]
```

Commit this decision so future developers understand why.

---

## Part 5: Proceed

Once decided, **execute decisively**:

- [ ] Implement chosen option
- [ ] Test thoroughly
- [ ] Update documentation
- [ ] Commit changes
- [ ] Move to next feature

**Don't second-guess.** You've measured, decided, now execute.

---

## Common Scenarios

### "We make generative art with short clips (< 60 seconds)"
→ **OPTION A** (Accept)
- Drift won't accumulate enough to matter
- Move on, focus on creative features

### "We do live performance with variable speed control"
→ **OPTION B** (Manual Resync)
- Allows artistic speed variations
- Users can resync if needed
- Good for performance flexibility

### "We're building a professional media playback tool"
→ **OPTION C** (Unified Timing)
- Users expect frame-perfect sync
- Worth the refactor
- Long-term investment

### "I'm not sure what Crumble will be used for yet"
→ **OPTION A** (Accept)
- Document the limitation
- Revisit when use case clarifies
- Don't over-engineer for unknown requirements

---

## Quick Reference: What Each Option Does

| Aspect | Option A | Option B | Option C |
|--------|----------|----------|----------|
| **Solves drift?** | No | Partially | Completely |
| **Implementation** | Docs | seek() API | Master clock |
| **User experience** | Auto | Manual control | Automatic |
| **Effort** | 30 min | 2-4 hrs | 3-5 days |
| **Maintenance** | None | Minimal | Ongoing |
| **Good for** | Short clips | Medium sessions | Critical sync |

---

## Red Flags: Don't Choose...

❌ Don't choose **Option C if**:
- You haven't confirmed drift is actually a problem
- Your sessions are < 2 minutes
- You're not sure what Crumble will be used for yet
- You just want to "fix it properly" without measured need

❌ Don't choose **Option B if**:
- You need completely automatic sync
- You're building a professional media tool
- Drift will accumulate over hours

❌ Don't choose **Option A if**:
- You've confirmed drift is unacceptable for your use case
- You're building professional media playback
- Users expect frame-perfect sync

---

## Final Checklist

Before moving on:

- [ ] I've measured actual drift
- [ ] I understand my use case
- [ ] I've chosen Option A, B, or C
- [ ] I've documented my decision
- [ ] I'm ready to implement
- [ ] I'm not second-guessing myself

When all boxes are checked: **You're ready to proceed.**

Good engineering requires making decisions with incomplete information, then executing decisively. You're doing it right.
