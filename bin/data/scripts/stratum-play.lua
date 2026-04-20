-- stratum-play.lua
-- DualSense gamepad → multi-layered AV loop scrubber
--
--   L3 Y           → speed (analog, pattern)
--   R3 X           → region start (analog, pattern)
--   R3 Y           → region width (analog, pattern)
--   L1 / R1        → batch size (digital, update)
--   L2 / R2        → scroll clips (digital, update, axes lt/rt)
--   Triangle       → cycle blend mode (digital, update)
--   Cross          → randomize (digital, update)
--   D-pad UP/DOWN  → opacity (digital, update)

local screen = videoout("screen"):on()
local vmx = vmix("videomix"):connect(screen):on()
local speakers = aout("speakers"):on()
local amx = amix("audiomix"):connect(speakers):on()

local bank = "superstratum_video-data"
local total = #(getBank(bank))

-- defaults
local batch = 1
local idx = 0
local opacity = 1
local blend = 1
local blends = { 0, 1, 2, 3 }  -- ALPHA, ADD, SCREEN
local frame = 0

-- joysticks
-- accum(init=0.5) so centered stick → neutral value:
--   speed: scale(-3,3) maps 0.5→0, so speed = 1.0 at rest
--   start: accum at 0.3 → loop starts near sample start
--   width: pow(0.5) for fine control at small sizes, scale floor = 0.0001
--   start + width: absolute endpoint via pattern composition (Sum pattern)
--   Use s:region(start, start + width) for atomic set
local spd = 1.0 + gpad("ly"):accum(-0.5, 0.5):scale(-3, 3)
local pos = gpad("rx"):accum(0.3, 0.5)
local lsz = gpad("ry"):accum(-0.3, 0.4):pow(2.0):scale(0.0001, 1)

local active = {}
local activeSet = {}

function update()
    frame = frame + 1

    if once("triangle") then blend = blend % #blends + 1 end
    if once("cross") then idx = math.random(0, total - 1) end
    if press("lt") then idx = (idx - 1 + total) % total end
    if press("rt") then idx = (idx + 1) % total end
    if press("l1") then batch = math.max(1, batch - 1) end
    if press("r1") then batch = math.min(total, batch + 1) end

    if held("up") then opacity = math.min(1, opacity + 0.02) end
    if held("down") then opacity = math.max(0, opacity - 0.02) end

    local function inBatch(clipIdx)
        for offset = 0, batch - 1 do
            if (idx + offset) % total == clipIdx then return true end
        end
        return false
    end

    for i = #active, 1, -1 do
        local entry = active[i]
        if not inBatch(entry.idx) then
            entry.node:destroy()
            activeSet[entry.idx] = nil
            table.remove(active, i)
        end
    end

    local layerOpacity = opacity / math.sqrt(batch)
    local layerGain = 1 / math.sqrt(batch)
    local blendMode = blends[blend]

    -- Apply blend/opacity/gain to all active samplers (not just newly spawned)
    for _, entry in ipairs(active) do
        entry.node.blend = blendMode
        entry.node.opacity = layerOpacity
        entry.node.gain = layerGain
    end

    for offset = 0, batch - 1 do
        local clipIdx = (idx + offset) % total
        if not activeSet[clipIdx] then
            local s = sampler(bank .. ":" .. clipIdx):on()
            s.speed = spd
            s:region(pos, pos + lsz)
            s.loop = true
            s.blend = blendMode
            s.opacity = layerOpacity
            s.gain = layerGain
            connect(s, vmx)
            connect(s, amx, { gain = layerGain })
            local entry = { node = s, idx = clipIdx }
            table.insert(active, entry)
            activeSet[clipIdx] = entry
        end
    end
end

print("stratum-play: " .. total .. " clips, batch=" .. batch)
