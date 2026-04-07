-- stratum-play.lua
-- DualSense gamepad → multi-layered AV loop scrubber
--
--   L3 Y           → speed (analog, pattern)
--   R3 X           → loop position (analog, pattern)
--   R3 Y           → loopSize (analog, pattern)
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
local batch = 3
local idx = 0
local opacity = 0.6
local blend = 1
local blends = { 0, 1, 2 }
local frame = 0

-- joysticks
local spd = 1.0 + gpad("ly"):accum(0.5):scale(-3, 3)
local pos = gpad("rx"):accum(0.5) -- seem like it doesn't work : maybe because we're not re-triggering samples currently ? do we even have proper loop in the script ?
local lsz = gpad("ry"):accum(0.25, 1)

local active = {}
local activeSet = {}

function update()
    frame = frame + 1

    local g = Gamepad or {}

    if press("cycle_blend", g.triangle or 0) then blend = blend % #blends + 1 end
    if press("randomize", g.cross or 0) then idx = math.random(0, total - 1) end
    if press("scroll_prev", g.lt or 0) then idx = (idx - 1 + total) % total end
    if press("scroll_next", g.rt or 0) then idx = (idx + 1) % total end
    if press("batch_down", g.l1 or 0) then batch = math.max(1, batch - 1) end
    if press("batch_up", g.r1 or 0) then batch = math.min(total, batch + 1) end

    if (g.up or 0) > 0.5 then opacity = math.min(1, opacity + 0.02) end
    if (g.down or 0) > 0.5 then opacity = math.max(0, opacity - 0.02) end

    for i = #active, 1, -1 do
        local entry = active[i]
        if entry.idx < idx or entry.idx >= idx + batch then
            entry.node:destroy()
            activeSet[entry.idx] = nil
            table.remove(active, i)
        end
    end

    local layerOpacity = opacity / math.sqrt(batch)
    local layerGain = 0.5 / math.sqrt(batch)
    local blendMode = blends[blend]

    for offset = 0, batch - 1 do
        local clipIdx = (idx + offset) % total
        if not activeSet[clipIdx] then
            local s = sampler(bank .. ":" .. clipIdx):on()
            s.speed = spd
            s.position = pos
            s.loop = true
            s.loopSize = lsz
            connect(s, vmx, { opacity = layerOpacity, blend = blendMode })
            connect(s, amx, { gain = layerGain })
            local entry = { node = s, idx = clipIdx }
            table.insert(active, entry)
            activeSet[clipIdx] = entry
        end
    end
end

print("stratum-play: " .. total .. " clips, batch=" .. batch)
