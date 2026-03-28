-- Crumble/b/d/s/main.lua
-- clear()
local screen = videoout():on()
local vmx = videomix():on():connect(screen)

local speakers = audioout():on()
local amx = audiomix():on():connect(speakers):gain(1) -- *midi(17,1)

bpm(200)

-- 2. Data-driven loading from AssetRegistry
local assets = getBank("superstratum_video-data")
local count = math.min(#assets, 64)

-- sine modulation for speed
local lfo1 = 1-- osc(1/128):scale(-1, 1)

-- 3. Declarative sampler creation
for i = 1, count do
    local asset = assets[i]
    
    -- noise(f, s): perlin drift
    -- local mix = noise(1, i)
    -- each sampler oscillates at i
    local mix =  gpad("triangle") + midi(82,1) + midinote(36,10) + miditouch(36,10)
    local gly = gpad("ly"):scale(1,-1)
    local glx = gpad("lx"):scale(-1,1)
    -- create samplers
    local s = sampler(asset.name):path(asset.path)
              :gain(mix)
              :opacity(mix)
              :speed(1+midi(74,1):scale(0,10) + gly + glx)
              :on()

    -- connect and configure mixer tracks
    local track = connect(s, { vmx, amx })
    local weight = 1.0 / (count / 3)

    if track then
        amx["gain_" .. track] = weight
        vmx["opacity_" .. track] = weight
        vmx["blend_" .. track] = 1 -- Additive/Alpha blend
    end
end

print(string.format("Loaded: %d active samplers from bank", count))
