-- drums.lua — Phasing noise from a single sample
clear()
local screen = videoout("screen"):on()
local vmx = videomix("mx"):connect(screen):on()
local speakers = audioout("speakers"):on()
local amx = audiomix("ax"):connect(speakers):on():gain(0.2)

-- 1. Pick a single source sample
local source = "drums:0" 

-- 2. Create voices of the SAME sample
local voices = 16

for i = 1, voices do
    local name = "v" .. i
    local s = sampler(name, {path = source}):on()
    
    -- Explicit slot index for bulletproof patching
    local track = i
    
    -- Clean declarative syntax: connect + modulate the connection
    s:connect(vmx, { 
        opacity = 1 / (voices/2),
        blend = 1
    }, track)

    s:connect(amx, {
        gain = 1 / (voices/2)
    }, track)
    
    -- Phasing: slight speed drift + noise per voice
    s.speed = 1.0 + (i * 0.001) -- * noise(i * 0.5):scale(0.8, 1.2)
    
    -- Rhythmic offset per voice
    --s.path = seq("0"):slow(i):shift(i * 0.125)
end

bpm(132)

print(string.format("DRUMS. Phasing %d voices of '%s'.", count, source))
