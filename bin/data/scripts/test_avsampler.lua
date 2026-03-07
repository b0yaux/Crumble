-- AVSampler test (Unified Assets)
-- clear()

-- 1. Create nodes and assign logical assets directly
-- Syntax: "bankName:index" or "bankName:fileName"
local s1 = addNode("AVSampler", "s1")
s1.path = "superstratum_video-data:4"

local s2 = addNode("AVSampler", "s2")
s2.path = "superstratum_video-data:50"

-- 2. Setup Routing
local amix = addNode("AudioMixer", "amix")
local vmix = addNode("VideoMixer", "vmix")
local speakers = addNode("SpeakersOutput", "speakers")
local screen = addNode("ScreenOutput", "screen")

connect({s1, s2}, amix)
connect({s1, s2}, vmix)
connect(amix, speakers)
connect(vmix, screen)

-- Use new PatternMath capabilities on speed:
-- speed: Smooth sine-wave fluctuation between 0.5x and 1.5x
-- s1.speed = scale(0.01, 0.6, osc(0.2))
 s1.speed = shift(0.7, seq("1 0 0 -0.5")) -- Offset phase of a sequence
-- s1.speed = fast(5, noise(1))            -- Very slow stochastic speed drift
-- s1.speed = 0.1
-- s2 speed: A quantized (snapped) sequence running at double time
-- s2.speed = fast(10, ramp(20)) -- 4-step staircase speed ramp
 s2.speed = scale(-1, 3, ramp(0.5))  -- Forward to backward sweep
-- s2.speed = 2.0   -- Fast jittery speed

s1:on()
s2:on()

-- 4. Mixer state
amix.volume = 0.1
vmix.blend_0, vmix.blend_1 = 0, 1

-- Custom logic & data persistence check
print("s1 path:", s1.path)
print("s2 path:", s2.path)

function update()
    if math.random() < 0.01 then
        print(string.format("Clock: %.2f | Cycle: %.2f", Time.absoluteTime, Time.cycle))
    end
end 
