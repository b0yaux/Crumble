-- Noûs: 3-minute evolution (Audio + Video parallel montage)
local DURATION = 180.0

local vmix = videomix("vmix"):on()
local amix = audiomix("audioMix"):on()
connect(vmix, videoout("viewport"):on())
connect(amix, audioout("audioOut"):on())

local audio = {}
local video = {}

for i, asset in ipairs(getBank("Noûs-performance-ka")) do
    local s = sampler(asset.name):path(asset.path):on()

    if asset.videoPath ~= "" then
        local layer = connect(s, vmix)
        table.insert(video, {node = s, layer = layer, name = asset.name, phase = i * 0.17})
    else
        connect(s, amix)
        table.insert(audio, s)
    end
end

function update()
    local phase = (Time.absoluteTime % DURATION) / DURATION
    local breathCap = math.sin(Time.absoluteTime * 0.3) * 0.1 + 0.1

    -- Audio: single -> dense -> single
    local aPos = phase * (#audio - 1)
    local aSpread = math.sin(phase * math.pi) + 0.5

    for i, s in ipairs(audio) do
        local vol = math.max(0, 1 - math.abs(i - 1 - aPos) / aSpread)
        if s.name == "04_Tout-Rien" then vol = math.min(vol, breathCap) end
        s.gain = vol
    end

    -- Video: white to black
    local maxOpacity = 1.0 / (#video / 3)
    local baseOp = (1 - phase) ^ 0.5 * maxOpacity
    local time = Time.absoluteTime

    for i, v in ipairs(video) do
        local mod = 1.0 + math.sin(time * 0.1 + v.phase * 6.28) * 0.2
        vmix["opacity_" .. v.layer] = baseOp * mod
        vmix["blend_" .. v.layer] = 1
    end
    
    for _, s in ipairs(audio) do
        print(string.format("[A] %s: %.2f", s.name, s.gain))
    end
    for _, v in ipairs(video) do
        print(string.format("[V] %s: op=%.3f blend=%d", v.name, vmix["opacity_" .. v.layer], vmix["blend_" .. v.layer]))
    end
    print("------ t=" .. string.format("%.1f", Time.absoluteTime % DURATION))
end
