-- minilab3-test.lua
-- Minilab 3 keyboard → gated voice sampler

---------------------------
-- Data setup
local bank = "feed"
local maxVoices = 64
local assets = getBank(bank)
local count  = #assets

-- Graph setup
local screen   = videoout():on()
local vmx      = videomix():on():connect(screen)
local speakers = audioout():on()
local amx      = audiomix():on():connect(speakers)

-- MINILAB3 bindings
-- knobs
local kb1 = midi(74, 1) -- pos
local kb2 = midi(71, 1):pow(2):scale(0.001, 1) -- lsz
local kb3 = midi(76, 1)
local kb4 = midi(77, 1)
local kb5 = midi(93, 1)
local kb6 = midi(18, 1)
local kb7 = midi(19, 1)
local kb8 = midi(16, 1)

-- sliders
local sl1 = midi(82, 1):scale(0.2, 2.5) -- speed
local sl2 = midi(83, 1)
local sl3 = midi(85, 1):scale(0.3, 1.0) -- mix
local sl4 = midi(17, 1):scale(0, 3):snap(4) -- blend

-- pads
local pad1 = midinote(36, 10)

--------------------------
-- Voice tracking: note number → { node, vel }
local voices = {}
local voiceCount = 0

--------------------------
-- Update loop
function update()
    for _, e in ipairs(midievents(1)) do
        if e.on and not voices[e.note] and voiceCount < maxVoices then
            local idx = e.note % count
            local s = sampler(bank .. ":" .. idx):on()
            s.speed = sl1
            s:mix(e.velocity * sl3)
            s.blend = sl4
            s.loop = true
            s:region(kb1, kb1 + kb2)
            connect(s, vmx); connect(s, amx)
            
            voices[e.note] = { node = s, vel = e.velocity }
            voiceCount = voiceCount + 1
            print(string.format("[voice +] note=%d vel=%.2f", e.note, e.velocity))
        elseif not e.on and voices[e.note] then
            voices[e.note].node:destroy()
            voices[e.note] = nil
            voiceCount = voiceCount - 1
            print(string.format("[voice -] note=%d", e.note))
        end
    end
end

print(string.format("minilab3-test: %d assets, max %d voices (midievents)",
    count, maxVoices))
print("Controls: Knob82=speed, Knob17=blend, Slider85=trim")
