-- Crumble: Synchronized A/V Folder Jam

-- 1. Setup Core Graph
local vMixer = addNode("VideoMixer", "VMixer")
local vOutput = addNode("ScreenOutput", "VOutput")
connect(vMixer, vOutput)

local aMixer = addNode("AudioMixer", "AMixer")
local aOutput = addNode("SpeakersOutput", "AOutput")

-- ROUTING: Tell the graph to pull audio from the mixer
setAudioOutput(aMixer)

-- CONNECT: Link mixer to the hardware output node
connect(aMixer, aOutput)

-- 2. Define Directory
local videoDir = "/Users/jaufre/works/superstratum_video-data"

-- 3. Use the listDir bridge directly for better control
local files = _listDir(videoDir)
local videoCount = 0

-- Iterate through all files to find .mov and check for .wav
for _, path in ipairs(files) do
    if path:match("%.mov$") then
        local idx = videoCount
        videoCount = videoCount + 1
        
        -- Add Video Node
        local vNode = addNode("VideoFileSource", "V_" .. idx)
        vNode.videoPath = path
        connect(vNode, vMixer, 0, idx)
        
        -- visual blending logic
        if idx == 0 then
            vMixer["opacity_" .. idx] = 1.0
            vMixer["blend_" .. idx] = 0 -- ALPHA
        else
            vMixer["opacity_" .. idx] = 0.3
            vMixer["blend_" .. idx] = 2 -- ADD
        end
        
        -- 4. Check for matching audio (.wav)
        local audioPath = path:gsub("%.mov$", ".wav")
        print("Checking for audio pair: " .. audioPath)
        
        if fileExists(audioPath) then
            print("FOUND audio for clip " .. idx)
            local aNode = addNode("AudioFileSource", "A_" .. idx)
            aNode.audioPath = audioPath
            connect(aNode, aMixer, 0, idx)
            aMixer["gain_" .. idx] = 0.5
        else
            print("MISSING audio for clip " .. idx)
        end
        
        -- Safety: limit to 10 layers for stability during tests
        if videoCount >= 10 then break end
    end
end

print("Jamming " .. videoCount .. " synchronized A/V layers.")
