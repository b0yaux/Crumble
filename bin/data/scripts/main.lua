-- Crumble: Live Audio/Video Folder Jam
clear()

-- 1. Setup Core Graph (Mixers and Outputs)
local vMixer = addNode("VideoMixer", "VMixer")
local vOutput = addNode("ScreenOutput", "VOutput")
connect(vMixer, vOutput)

local aMixer = addNode("AudioMixer", "AMixer")
local aOutput = addNode("SpeakersOutput", "AOutput")
connect(aMixer, aOutput)

-- 2. Import clips and orchestrate parallel playback
local videoDir = "/Users/jaufre/works/superstratum_video-data"
local videoNodes = importFolder(videoDir, ".mov")

for i, vNode in ipairs(videoNodes) do
    local idx = i - 1
    
    -- Route Video to Video Mixer
    connect(vNode, vMixer, 0, idx)
    vMixer["opacity_" .. idx] = (idx == 0) and 1.0 or 0.3
    
    -- 3. Parallel Audio: Look for matching .wav file
    local audioPath = vNode.videoPath:gsub("%.mov$", ".wav")
    
    if fileExists(audioPath) then
        local aNode = addNode("AudioFileSource", "Audio_" .. i)
        aNode.audioPath = audioPath
        
        -- Route to Audio Mixer
        connect(aNode, aMixer, 0, idx)
        aMixer["gain_" .. idx] = 0.5
    end
end

print("Jamming " .. #videoNodes .. " synchronized A/V layers.")
