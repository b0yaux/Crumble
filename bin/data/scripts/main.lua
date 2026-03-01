-- Crumble
-- Entry point: loaded via config.json entryScript
-- Uses loader.lua for directory scanning

local loader = require("loader")

-- 1. Setup Core Graph
local vMixer = addNode("VideoMixer", "VMixer")
local vOutput = addNode("ScreenOutput", "VOutput")
connect(vMixer, vOutput)

local aMixer = addNode("AudioMixer", "AMixer")
local aOutput = addNode("SpeakersOutput", "AOutput")
connect(aMixer, aOutput)
aOutput.volume = 1.0


-- 2. Load directory - generic loader returns by extension
local workDir = "/Users/jaufre/works/superstratum_video-data"
local data = loader.scan(workDir, { limit = 64 })

-- 3. Caller decides what to do with the data
local videos = data[".mov"] or {}
local audios = data[".wav"] or {}



-- 4. Create video nodes
for i, video in ipairs(videos) do
    local idx = i - 1
    local name = video.name or "clip" .. idx
    
    -- Video node
    local vNode = addNode("VideoFileSource", "v" .. idx .. "_" .. name)
    vNode.path = video.path
    connect(vNode, vMixer, 0, idx)
    
    -- Blending logic - equal distribution
    local opacity = 1.0 / #videos
    local blend = 1  -- ADD blend for layered look
    vMixer["opacity_" .. idx] = opacity
    vMixer["blend_" .. idx] = blend
    
    -- Find audio pair (caller's responsibility)
    local audioPath = video.path:gsub("%.mov$", ".wav")
    if fileExists(audioPath) then
        local aNode = addNode("AudioFileSource", "a" .. idx .. "_" .. name)
        aNode.path = audioPath
        connect(aNode, aMixer, 0, idx)
        aMixer["gain_" .. idx] = 0.5
    end
end

print("Loaded " .. #videos .. " video layers, " .. #audios .. " audio files")
