-- AVSampler test
clear()

local path = "/Users/jaufre/works/superstratum_video-data"
local files = _listDir(path)
local videos = {}
for _, f in ipairs(files) do
    if type(f) == "string" and f:match("%.mov$") then 
        table.insert(videos, f) 
    end
end

local function smp(i)
    local v = videos[i]
    if not v or type(v) ~= "string" then return end
    local n = addNode("AVSampler", "s" .. i)  -- named for idempotency
    n.videoPath = v
    local wav = v:gsub("%.mov$", ".wav")
    if fileExists(wav) then n.audioPath = wav end
    n.loop = true
    n.playing = true
    return n
end

local s1, s2 = smp(89), smp(22)

local amix = addNode("AudioMixer", "amix")
local vmix = addNode("VideoMixer", "vmix")
local speakers = addNode("SpeakersOutput", "speakers")
local screen = addNode("ScreenOutput", "screen")

connect({s1, s2}, amix)
connect({s1, s2}, vmix)
connect(amix, speakers)
connect(vmix, screen)


-- Sample-accurate sequencing using the new C++ Generator engine
s1.speed = seq("0.1 -1 1 1 1")
s2.speed = seq("1 0.2 3.3 1 0.5")
s1.volume = 0.01
s2.volume = 0.01

amix.gain_0, amix.gain_1 = 1, 1
vmix.opacity_0, vmix.opacity_1 = 1, 1
vmix.blend_0, vmix.blend_1 = 0, 1

print("s1:", s1 and s1.videoPath)
print("s2:", s2 and s2.videoPath)

function update()
    -- Macroscopic UI/Video timing still works here if needed
    if math.random() < 0.01 then
        print(string.format("Clock: %.2f | Cycle: %.2f", Time.absoluteTime, Time.cycle))
    end
end
