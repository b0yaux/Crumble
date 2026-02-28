-- Test script for AVSampler node
-- Demonstrates unified audio/visual sampling driven by the Transport C++ loop

local P = require("lib.patterns")

-- Create AVSampler instance
local sampler = addNode("AVSampler", "avsampler")

-- Load real media files from superstratum_video-data
local mediaPath = "/Users/jaufre/works/superstratum_video-data"
sampler.videoPath = mediaPath .. "/0-playing-around-with-a-CRT-projector_clip_00.mov"
sampler.audioPath = mediaPath .. "/0-playing-around-with-a-CRT-projector_clip_00.wav"

-- Set initial static parameters
sampler.loop = true
sampler.playing = true

-- Create output nodes
local screen = addNode("ScreenOutput", "screen")
local speakers = addNode("SpeakersOutput", "speakers")

-- Connect sampler to outputs
connect(sampler, speakers, 0, 0)  -- audio
connect(sampler, screen, 0, 0)    -- video

print("=== AVSampler Transport Test ===")
print("Video path: " .. (sampler.videoPath or "nil"))
print("Audio path: " .. (sampler.audioPath or "nil"))

-- Loop Phase: Called automatically by C++ at 60fps
function update(t)
    -- Manipulate the sampler playback speed dynamically using the mini-Tidal parser!
    -- Cycle: 1 beat plays forward (1.0), reverse (-1.0), half speed (0.5), and double speed (2.0)
    local nextSpeed = P.step(t.cycle, "6 -1 -0.5 0.8 1.2")
    sampler.speed = nextSpeed
    
    -- Animate the volume between 0 and 0.5 using an LFO synchronized to the transport
    local nextVolume = P.osc(t.cycle, 1.0) * 0.5
    sampler.volume = 1.0
    
    -- Print current transport state VERY OFTEN to debug
    if math.random() < 0.05 then
        print(string.format("Time: %.2f | Cycle: %.2f | Speed: %.2f | Volume: %.2f", 
              t.absoluteTime, t.cycle, nextSpeed or 0, nextVolume or 0))
    end
end
