-- test-minimal.lua
-- 1 sampler, static values, no gamepad, no update()
-- If this produces audio+video, the C++ pipeline works.

local screen = videoout("screen"):on()
local vmix = vmix("vmix"):connect(screen):on()
local speakers = aout("speakers"):on()
local amix = amix("amix"):connect(speakers):on()

local s = sampler("superstratum_video-data:0"):connect({vmix, amix}):on()
s.speed = 1.0
s.loop = true
s:region(0, 1)

print("test-minimal: 1 sampler, static values")
