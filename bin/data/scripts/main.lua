-- Crumble
-- Entry point: loaded via config.json entryScript
-- Uses registry-based asset loading with AVSampler for A/V sync

--clear()

-- 1. Setup Core Graph
local vmix = addNode("VideoMixer", "vmix")
local screen = addNode("ScreenOutput", "screen")
connect(vmix, screen)

local amix = addNode("AudioMixer", "amix")
local speakers = addNode("SpeakersOutput", "speakers")
connect(amix, speakers)
speakers.gain = 0.1

bpm(200)

-- 2. Data-driven loading from AssetRegistry
local assets = getBank("superstratum_video-data")
local count = math.min(#assets, 5)

local lfo1 = osc(3/10)
local lfo2 = 1 -- scale(0.1, 2, osc(3/100))

    -- 3. Create AVSampler nodes with staggered modulations
for i = 1, count do
    local asset = assets[i]
    local idx = i - 1
    
    local sampler = addNode("AVSampler", string.format("s%d_%s", idx, asset.name))
    sampler.path = asset.path
    
    -- rand(s): Returns a static number (0-1) for per-voice variation
    -- noise(f, s): Returns a dynamic pattern for smooth organic drift
    local mix = noise(0.5, i * 0.1)
    
    sampler:gain(mix):opacity(mix)
        :speed(lfo1)
    
    local layer = connect(sampler, {vmix, amix})
    
    -- Configure layer parameters using the assigned layer index
    if layer then
        local weight = 1.0 / (count / 3)
        amix["gain_" .. layer] = weight
        vmix["opacity_" .. layer] = weight
        vmix["blend_" .. layer] = 1
    end
end

print(string.format("Loaded %d AVSamplers from bank 'superstratum_video-data'", count))
