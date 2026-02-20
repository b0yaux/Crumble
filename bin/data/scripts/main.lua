-- Crumble: Live Folder Jam
clear()

local mixer = addNode("VideoMixer", "Mixer")
local output = addNode("ScreenOutput", "Output")

connect(mixer, output)

-- 1. Import all clips from a directory
-- (Note: you can change this to any valid directory)
local videoDir = "/Users/jaufre/works/superstratum_video-data"
local clips = importFolder(videoDir, ".mov")

-- 2. Mix them all into the mixer
for i, node in ipairs(clips) do
    local idx = i - 1
    connect(node, mixer, 0, idx)
    
    if idx == 0 then
        mixer["opacity_" .. idx] = 0.01
        mixer["blend_" .. idx] = 1 -- ALPHA
    else
        mixer["opacity_" .. idx] = 0.2
        mixer["blend_" .. idx] = 2 -- ADD
    end
end

print("Imported " .. #clips .. " clips from " .. videoDir)
