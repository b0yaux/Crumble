-- Crumble
-- Entry point: loaded via config.json entryScript
-- Uses registry-based asset loading with AVSampler for A/V sync

clear()

-- 1. Setup Core Graph
local vmix = addNode("VideoMixer", "vmix")
local screen = addNode("ScreenOutput", "screen")
connect(vmix, screen)

local amix = addNode("AudioMixer", "amix")
local speakers = addNode("SpeakersOutput", "speakers")
connect(amix, speakers)
speakers.volume = 1.0

-- 2. Data-driven loading from AssetRegistry
local bankName = "superstratum_video-data"
local assets = getBank(bankName)
local maxClips = math.min(#assets, 19)

-- 3. Create AVSampler nodes for unified A/V playback
for i = 1, maxClips do
    local asset = assets[i]
    local idx = i - 1  -- 0-indexed for layer assignment
    
    -- Node name is strictly its ID + filename for clarity
    local nodeName = string.format("s%d_%s", idx, asset.name)
    
    -- Use AVSampler with registry path syntax
    local sampler = addNode("AVSampler", nodeName)
    sampler.path = asset.path
    
    -- Route to mixers (Auto-Indexing handles the layer assignment and returns it)
    -- We assign the result to 'layer' to use it below.
    local layer = connect(sampler, {vmix, amix})
    
    -- Configure layer parameters using the assigned layer index
    if layer then
        local opacity = 1.0 / (maxClips / 3)
        vmix["opacity_" .. layer] = opacity
        vmix["blend_" .. layer] = 1 
        amix["gain_" .. layer] = 0.5 
    end
end

print(string.format("Loaded %d AVSamplers from bank '%s'", maxClips, bankName))
