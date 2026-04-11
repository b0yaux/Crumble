-- AVSampler test — pattern triggers & modulations
-- Tests: logical assets, method-chained pattern transforms, mixer routing
--clear() 
local ao = aout():on()
local vo = vout():on()
local amix = amix():connect(ao)
local vmix = vmix():connect(vo)
local avmix = {vmix,amix}

local s1 = sampler("drums:0"):connect(avmix)
:speed(seq("1 -0.9 1.2 -0.5"):shift(0.7)):off()

local s2 = sampler("drums:1"):connect(avmix):blend(1)
:speed(seq("1 0.5"):shift(2)):off()

-- osc(0.2):scale(0.01, 0.6)
-- noise(1):fast(5)
-- ramp(0.5):scale(-1, 3)

-- T1 standalone audio — self-triggering via path pattern (dynamic switching)
local a1 = audio("drums:0"):connect(amix)
a1.path = seq("drums:1 ~ drums:0 ~")

-- T1 standalone video — self-triggering via path pattern
local v1 = video("travaux"):connect(vmix)
v1.path = seq("travaux ~ ~ travaux")

-- T1 playhead check
function update()
    print("playhead a1:", playhead(a1))
    print("playhead v1:", playhead(v1))
end