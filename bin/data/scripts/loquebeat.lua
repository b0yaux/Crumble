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
local drum = sampler("k s k"):fast(4):connect(avmix):blend(1)
    :mix(1):on():position(0.2)
-- local trvx = sampler("travaux"):connect(avmix):blend(1)
   -- :mix(seq("1 0.3"):fast(lfo1)):on()
    --:path(seq("0"):fast(1))
    
local clip = s("muzicvids:3"):connect(avmix):blend(1)    