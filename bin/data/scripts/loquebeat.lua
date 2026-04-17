-- loquebeat.lua - Test embedded audio+video from single .mov file
-- Test alias + pattern syntax

-- Setup outputs
local screen = videoout():on()
local vmix = videomix():on():connect(screen)
local speakers = audioout():on()
local amix = audiomix():on():connect(speakers):gain(0.8)

local avmix = {vmix,amix}

-- Setup aliases
alias("t", "travaux")
aliases({
    k = "drums:0",
    s = "drums:1"
})

local lfo1 = sine(1/16):scale(0,64)

bpm(120)
-- Create samplers with presentation settings
-- local d = delay():on():time(0.375):feedback(0.7):wet(0.8):connect(amix)

local drum = sampler(seq("s ~ s k k"):fast(2))
    :connect(avmix)
    :blend(1):mix(1):off()
    :position(osc(1/2):shift(0.21))
    :speed(0.67)


local clip = s("muzicvids:10"):connect(avmix):blend(1):off()  

local clip = s("rnd:3"):connect(avmix):blend(1):on()   

-- local trvx = sampler("travaux"):connect(avmix):blend(1)
   -- :mix(seq("1 0.3"):fast(lfo1)):on()
    --:path(seq("0"):fast(1))
    