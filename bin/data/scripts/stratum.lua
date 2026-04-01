-- Crumble
-- Data-driven AVSampler script: loads all assets from a bank

-- 1. Setup Core Graph
local screen = videoout("screen"):on()
local vmix = videomix("vmix"):connect(screen):on()

local speakers = audioout("speakers"):on()
local amix = audiomix("amix"):connect(speakers):on()

-- 2. Data-driven loading from AssetRegistry
local bankName = "superstratum_video-data"
local assets = getBank(bankName)
local maxClips = math.min(#assets, 64)

-- 3. Create AVSampler nodes for unified A/V playback
 for i = 1, maxClips do
    local asset = assets[i]
    local idx = i - 1
    local s = sampler(asset.name,{path=asset.path}):on()

    local layer = connect(s, {vmix, amix})

    if layer then
        local opacity = 1.0 / (maxClips / 3)
        vmix["opacity_" .. layer] = opacity
        vmix["blend_" .. layer] = 1
        amix["gain_" .. layer] = 0.5
    end
end

print(string.format("Loaded %d AVSamplers from bank '%s'", maxClips, bankName))
