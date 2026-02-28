-- AVSampler test

require("lib.patterns")

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

local s1, s2 = smp(7), smp(13)

local amix = addNode("AudioMixer", "amix")
local vmix = addNode("VideoMixer", "vmix")
local speakers = addNode("SpeakersOutput", "speakers")
local screen = addNode("ScreenOutput", "screen")

connect(s1, amix, 0, 0)
connect(s2, amix, 0, 1)
connect(s1, vmix, 0, 0)
connect(s2, vmix, 0, 1)
connect(amix, speakers)
connect(vmix, screen)

amix.gain_0, amix.gain_1 = 1, 1
vmix.opacity_0, vmix.opacity_1 = 1, 1
vmix.blend_0, vmix.blend_1 = 0, 1

print("s1:", s1 and s1.videoPath)
print("s2:", s2 and s2.videoPath)

function update()
    if s1 then s1.speed = step("0.1 -1 1") end
    if s2 then s2.speed = step("1 -2 0.2") end
end
