-- AVSampler test (Unified Assets)
clear()

-- 1. Create nodes and assign logical assets directly
-- Syntax: "bankName:index" or "bankName:fileName"
-- The .path setter handles both Video and Audio pairing automatically via C++ VFS.
local s1 = addNode("AVSampler", "s1")
s1.path = "superstratum_video-data:92"

local s2 = addNode("AVSampler", "s2")
s2.path = "superstratum_video-data:29"

-- 2. Setup Routing
local amix = addNode("AudioMixer", "amix")
local vmix = addNode("VideoMixer", "vmix")
local speakers = addNode("SpeakersOutput", "speakers")
local screen = addNode("ScreenOutput", "screen")

connect({s1, s2}, amix)
connect({s1, s2}, vmix)
connect(amix, speakers)
connect(vmix, screen)

-- 3. Sample-accurate sequencing
s1.speed = seq("0.1 -1 -1 1 1")
s2.speed = seq("1 0.2 3.3 1 0.5")
s1.volume, s2.volume = 0.01, 0.01

-- 4. Mixer state
amix.gain_0, amix.gain_1 = 1, 1
vmix.opacity_0, vmix.opacity_1 = 1, 1
vmix.blend_0, vmix.blend_1 = 0, 1

-- Custom logic & data persistence check
print("s1 path:", s1.path)
print("s2 path:", s2.path)

function update()
    if math.random() < 0.01 then
        print(string.format("Clock: %.2f | Cycle: %.2f", Time.absoluteTime, Time.cycle))
    end
end 
