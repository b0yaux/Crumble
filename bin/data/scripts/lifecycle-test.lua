-- lifecycle-test.lua
-- Proof of concept: dynamic node creation/destruction from update()
-- Tests: birth, destroy, and continuous evolution

local ao = aout():on()
local vo = vout():on()
local amix = amix():connect(ao)
local vmix = vmix():connect(vo)
local avmix = {vmix, amix}

bpm(120)

local bank = getBank("superstratum_video-data")
local nodes = {}
local max_pop = 64
local next_spawn = 1
local spawn_every = 0.01
local die_every = 0.2
local next_die = die_every

function update()
    local bar = Time.bars

    if bar >= next_spawn and #nodes < max_pop then
        local i = (#nodes % #bank) + 1
        local asset = bank[i]
        local node = sampler(asset.name):connect(avmix):mix(0.5):on()
        table.insert(nodes, {node = node, born = bar})
        next_spawn = bar + spawn_every
        print("[BORN] " .. asset.name .. " (pop=" .. #nodes .. ") bar=" .. string.format("%.2f", bar))
    end

    if bar >= next_die and #nodes > 1 then
        local entry = table.remove(nodes, 1)
        entry.node:destroy()
        next_die = bar + die_every
        print("[DIED] (pop=" .. #nodes .. ") bar=" .. string.format("%.2f", bar))
    end
end
