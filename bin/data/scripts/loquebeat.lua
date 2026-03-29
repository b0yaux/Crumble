-- loquebeat.lua - Test embedded audio+video from single .mov file
-- Test alias + sp() pattern syntax

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

lfo1 = sine(1/16):scale(0,64)

bpm(120)
-- Create samplers with presentation settings
local drum = sampler("drums:0"):connect(avmix):blend(1)
:n(seq("0"):fast(2)):mix(1)
local trvx = sampler("travaux"):connect(avmix):blend(1)
:n(seq("0"):slow(4)):mix(seq("1 0.3"):fast(lfo1)) -- Beats 2 & 4