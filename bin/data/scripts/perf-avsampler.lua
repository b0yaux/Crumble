-- perf-avsampler.lua
-- A/B performance test: C++ AVSampler vs Lua sub-graph
-- Toggle USE_SUBGRAPH to switch between paths.
-- Adjust MAX_CLIPS (64 or 120) and USE_PATTERNS.
-- Monitor frame times in console (C++ instrumentation).

local USE_SUBGRAPH = true
local MAX_CLIPS = 120
local USE_PATTERNS = true

local screen = videoout("screen"):on()
local vmix = videomix("vmix"):connect(screen):on()

local speakers = audioout("speakers"):on()
local amix = audiomix("amix"):connect(speakers):on()

local bankName = "superstratum_video-data"
local assets = getBank(bankName)
local maxClips = math.min(#assets, MAX_CLIPS)

local mode = USE_SUBGRAPH and "SUB-GRAPH" or "C++"
local pats = USE_PATTERNS and "+patterns" or "+static"
print(string.format("[PERF] Mode: %s %s | Bank: %s | Clips: %d", mode, pats, bankName, maxClips))

for i = 1, maxClips do
    local asset = assets[i]
    local s
    if USE_SUBGRAPH then
        s = graph(asset.name, {script = "scripts/nodes/avsampler.lua"})
    else
        s = sampler(asset.name)
    end
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

print(string.format("[PERF] %d nodes created. Watch console for frame-time logs.", maxClips))
