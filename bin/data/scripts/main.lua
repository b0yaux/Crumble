-- Crumble
-- Performance Script: Live-loading via main.lua

-- 1. Setup Core Graph
local screen = videoout():on()
local vmx = vmix():on():connect(screen)

local speakers = audioout():on()
local amx = amix():on():gain(0.1):connect(speakers)

bpm(200)

-- 2. Data-driven loading from AssetRegistry
local assets = getBank("superstratum_video-data")
local count = math.min(#assets, 5)

-- noise(f, s): Returns a dynamic pattern for smooth organic drift
local lfo1 = osc(3/20):scale(-5, 5)

-- 3. Declarative AVSampler creation
for i = 1, count do
    local asset = assets[i]
    local mix = noise(0.5, i * 0.1)
    local weight = 1.0 / (count / 3)

    -- Create AVSampler with table constructor for path
    local s = sampler(asset.name, { path = asset.path })
              :gain(mix)
              :opacity(mix)
              :speed(lfo1)
              :on()

    -- Connect and configure mixer slots using the returned index
    local layer = connect(s, { vmx, amx })
    
    if layer then
        amx["gain_" .. layer] = weight
        vmx["opacity_" .. layer] = weight
        vmx["blend_" .. layer] = 1 -- Additive/Alpha blend
    end
end

print(string.format("Loaded: %d active samplers from bank", count))
