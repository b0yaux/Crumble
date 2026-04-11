-- perf-avsampler.lua
-- Performance test for Lua sub-graph AVSampler (sampler() = graph node).
-- Adjust MAX_CLIPS and USE_PATTERNS.
-- Requires CRUMBLE_PERF 1 in Graph.cpp for frame-time logs.

local MAX_CLIPS = 120
local USE_PATTERNS = true

local screen = videoout("screen"):on()
local vmix = videomix("vmix"):connect(screen):on()

local speakers = audioout("speakers"):on()
local amix = audiomix("amix"):connect(speakers):on()

local bankName = "superstratum_video-data"
local assets = getBank(bankName)
local maxClips = math.min(#assets, MAX_CLIPS)

local pats = USE_PATTERNS and "+patterns" or "+static"
print(string.format("[PERF] sampler() (sub-graph) %s | Bank: %s | Clips: %d", pats, bankName, maxClips))

for i = 1, maxClips do
    local asset = assets[i]
    local s = sampler(asset.name)
    s.path = asset.path

    if USE_PATTERNS then
        s:mix(seq("1 0.5 0.3 ~"):fast(math.random(1, 4)))
        s:blend(seq("1 0 2 0"):slow(math.random(2, 8)))
        if i % 3 == 0 then
            s.path = seq(string.format("%s ~ ~", asset.path)):slow(math.random(1, 4))
        end
    end

    s:on()

    local layer = connect(s, {vmix, amix})

    if layer then
        local opacity = 1.0 / (maxClips / 3)
        vmix["opacity_" .. layer] = opacity
        vmix["blend_" .. layer] = 1
        amix["gain_" .. layer] = 0.5
    end
end

print(string.format("[PERF] %d samplers created. Watch console for PERF: logs.", maxClips))
