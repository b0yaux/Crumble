-- Crumble: Stabilized 6-Layer Jam
clear()

-- 1. Setup Core Graph
local mixer = addNode("VideoMixer", "Mixer")
local output = addNode("ScreenOutput", "Output")

-- 2. Define Videos
local videoFiles = {
    "/Users/jaufre/works/superstratum_video-data/Bespoke-The-New-Free-Modular-Synthetizer-DAW-Tutorial-on-How-To-Use-BESPOKE-1-0-IS-OUT_clip_8.mov",
    "/Users/jaufre/works/superstratum_video-data/birds-1-65iyZE-msMk_clip_2.mov",
    "/Users/jaufre/works/superstratum_video-data/BIOMIMICRY-ansGoSRhWyA_clip_55.mov",
    "/Users/jaufre/works/superstratum_video-data/Bo-Burnham-The-Neoliberal-Performing-Self_clip_6.mov",
    "/Users/jaufre/works/superstratum_video-data/Collecting-Information-SBRB-Vma3-U_clip_63.mov",
    "/Users/jaufre/works/superstratum_video-data/cozzzzz-8uEmQV3ZpO0_clip_62.mov"
}

-- 3. Dynamic Loading & Connections
for i, path in ipairs(videoFiles) do
    local idx = i - 1
    local v = addNode("VideoFileSource", "V" .. i)
    
    -- Load and play
    v.videoPath = path
    v.loop = true
    
    -- Route to mixer input idx
    connect(v, mixer, 0, idx)
    
    -- Configure mixer layer properties
    mixer["active_" .. idx] = true
    
    -- First layer is the base (ALPHA), others are blended in (ADD)
    if idx == 0 then
        mixer["opacity_" .. idx] = 1.0
        mixer["blend_" .. idx] = 0 -- ALPHA
    else
        mixer["opacity_" .. idx] = 0.1 -- Lower opacity for ADD to prevent washout
        mixer["blend_" .. idx] = 2 -- ADD
    end
end

-- Final connection to screen
connect(mixer, output)

print("Jam Stabilized: 6 layers loaded and routed.")
