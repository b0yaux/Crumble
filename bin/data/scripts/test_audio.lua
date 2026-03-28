-- test_audio.lua — Minimal test: play embedded audio with no triggers, no patterns
-- Bypasses the entire trigger/mute system to isolate the decode+playback pipeline

local speakers = audioout():on()
local amix = audiomix():on():connect(speakers):gain(1.0)

local s = sampler("drums:0"):connect(amix):on()
s.playing = true
s.speed = 1.0
s.gain = 1.0
s.loop = true
