-- checkpoint-avsampler.lua
-- Checkpoint validation: Lua sub-graph AVSampler vs C++ AVSampler
-- Tests C1-C6 from the roadmap checkpoint spec.
--clear()
local ao = aout():on()
local vo = vout():on()
local amix = amix():connect(ao)
local vmix = vmix():connect(vo)
local avmix = {vmix, amix}

bpm(100)
-- sampler 1: static path
local s1 = graph("clip1", {script = "scripts/nodes/avsampler.lua"}):connect(avmix)
   :path("drums:0"):on():mix(0.5):blend(1)
   :speed(seq("1 -0.9 1.2 -0.5"):shift(0.7))

-- sampler 2: drums
local s2 = graph("clip2", {script = "scripts/nodes/avsampler.lua"}):connect(avmix):blend(1)
    :path(seq("drums:0 ~ drums:0 ~ ~"):slow(9)):mix(gpad("circle"))
    --:mix(noise(1)) -- gain + opacity modulation

-- sampler 3: travaux
local s3 = graph("clip3", {script = "scripts/nodes/avsampler.lua"}):connect(avmix):path(seq"travaux ~ ~  ~"):on():blend(2):speed(0.8)

-- sampler 4: drums
local s4 = graph("clip4", {script = "scripts/nodes/avsampler.lua"}):connect(avmix):path("drums:0"):on():blend(1):mix(0.3)
    :speed(seq("1 0.9 1.2 0.1"):scale(-2,0.5))

-- T1 standalone references for comparison
-- local a1 = audio("drums:0"):connect(amix)
-- a1.path = seq("drums:1 ~ drums:0 ~")

-- local v1 = video("travaux"):connect(vmix)
-- v1.path = seq("travaux ~ ~ travaux")

--local s1 = sampler("drums:0"):connect(avmix):mix(0.5)
--local s2 = sampler("drums:0"):connect(avmix):mix(0.5):speed(-1)

function update()
    -- Playhead sync — verified in update() via playhead() readout
    -- print("playhead s1 (sub-graph):", playhead(s1))
    -- print("playhead s2 (sub-graph):", playhead(s2))
end
