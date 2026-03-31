-- checkpoint-avsampler.lua
-- Checkpoint validation: Lua sub-graph AVSampler vs C++ AVSampler
-- Tests C1-C6 from the roadmap checkpoint spec.
-- clear()
local ao = aout():on()
local vo = vout():on()
local amix = amix():connect(ao)
local vmix = vmix():connect(vo)
local avmix = {vmix, amix}

-- C1: Static path load — graph() with script, then set path
local s1 = graph("clip1", {script = "scripts/nodes/avsampler.lua"}):connect(avmix):off()
s1.path = "drums:0"

-- C2: Pattern triggers — dynamic sample switching via path pattern
local s2 = graph("clip2", {script = "scripts/nodes/avsampler.lua"}):connect(avmix):blend(1):on()
s2.path = seq("drums:1 drums:0")


-- C3: Speed modulation
s1.speed = seq("1 -0.9 1.2 -0.5"):shift(0.7)

-- C4: Mix fan-out — gain on audio, opacity on video
-- s2:mix(seq("1 0.5 0"))

-- C5: Playhead sync — verified in update() via playhead() readout

-- C6: Connection to mixer — already done above via connect(s, avmix)

-- T1 standalone references for comparison
-- local a1 = audio("drums:0"):connect(amix)
-- a1.path = seq("drums:1 ~ drums:0 ~")

-- local v1 = video("travaux"):connect(vmix)
-- v1.path = seq("travaux ~ ~ travaux")

function update()
    print("playhead s1 (sub-graph):", playhead(s1))
end
