-- subgraph-test.lua
-- Validation: Lua sub-graph sampler syntax
-- Tests static path, pattern path, speed modulation, mix, blend, playhead sync.
-- clear()
-- !!!!!!!!!!
-- live-reload isn't glitch-free even while clear() is commented out ! user hitting cmd+S to-save create a very perceptible audio+video glitch -> we should investigate why is that happening.

local ao = aout():on()
local vo = vout():on()
local amix = amix():connect(ao)
local vmix = vmix():connect(vo)
local avmix = {vmix, amix}

bpm(60)

 local s1 = sampler("drums:0"):connect(avmix):on()
    :mix(0.5):blend(1)
    --:speed(seq("1 -0.9 1.2 -0.5"):scale(-0.5,0.5))
 
 local s2 = sampler("drums"):connect(avmix):blend(1):on()
    :path(seq("0 ~ 1 ~ ~"):fast(2))
    --:speed(0.5):position(osc(1/4)) -- 'position' pattern is evaluated     at frame-rate
--    :mix(gpad("circle")

    -- travaux
 local s3 = sampler("travaux"):connect(avmix)
    :path(seq("travaux ~")):off():blend(0):speed(0.8):position(0.6)

 local s4 = sampler("drums:0"):connect(avmix)
    :off():blend(1):mix("0.3 0.9")
    :speed(seq("1 0 0.2 2"):scale(-2, 0.5):fast(2))
    
function update()
end
