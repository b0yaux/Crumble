-- Crumble
-- Entry point: loaded via config.json entryScript
-- Use require() to load other modules:
--   require("infrastructure")  -- mixers, outputs
--   require("sources")         -- video files
--   require("effects")         -- optional

-- 1. Setup Core Graph
local vMixer = addNode("VideoMixer", "VMixer")
local vOutput = addNode("ScreenOutput", "VOutput")
connect(vMixer, vOutput)

local aMixer = addNode("AudioMixer", "AMixer")
local aOutput = addNode("SpeakersOutput", "AOutput")
connect(aMixer, aOutput)
aOutput["volume"] = 0

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
        
        -- Extract filename without extension for node name
        local filename = path:match("([^/]+)%.mov$")
        if not filename then
            filename = "v" .. idx
        else
            filename = "v" .. idx .. "_" .. filename
        end
        
        -- Add Video Node
        local vNode = addNode("VideoFileSource", filename)
        vNode.path = path
        connect(vNode, vMixer, 0, idx)
        
        -- visual blending logic
        if idx == 0 then
            vMixer["opacity_" .. idx] = 0.02
            vMixer["blend_" .. idx] = 0 -- ALPHA
        else
            vMixer["opacity_" .. idx] = 0.038
            vMixer["blend_" .. idx] = 1 -- ADD
        end
        
        -- 4. Check for matching audio (.wav)
        local audioPath = path:gsub("%.mov$", ".wav")
        print("Checking for audio pair: " .. audioPath)
        
        if fileExists(audioPath) then
            print("FOUND audio for clip " .. idx)
            local audioFilename = audioPath:match("([^/]+)%.wav$")
            if not audioFilename then
                audioFilename = "a" .. idx
            else
                audioFilename = "a" .. idx .. "_" .. audioFilename
            end
            local aNode = addNode("AudioFileSource", audioFilename)
            aNode.path = audioPath
            connect(aNode, aMixer, 0, idx)
            aMixer["gain_" .. idx] = 0.5
        else
            print("MISSING audio for clip " .. idx)
        end
        
        -- Safety: limit to 10 layers for stability during tests
        if videoCount >= 88 then break end
    end
end

print("Jamming " .. videoCount .. " synchronized A/V layers.")
