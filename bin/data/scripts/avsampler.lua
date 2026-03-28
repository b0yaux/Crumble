-- AVSampler test — pattern triggers & modulations
-- Tests: logical assets, method-chained pattern transforms, mixer routing
clear() 
local ao = aout():on()
local vo = vout():on()
local amix = amix():connect(ao)
local vmix = vmix():connect(vo)
local avmix = {vmix,amix}

local s1 = sampler("drums:0"):connect(avmix)
:speed(seq("1 -0.9 1.2 -0.5"):shift(0.7))

local s2 = sampler("drums:1"):connect(avmix):blend(1)
:speed(seq("1 0.5"):shift(2))

-- osc(0.2):scale(0.01, 0.6)
-- noise(1):fast(5)
-- ramp(0.5):scale(-1, 3)