-- lifecycle-stress.lua
-- Stress test: rapid create/destroy to trigger SIGSEGV

local ao = aout():on()
local vo = vout():on()
local amix = amix():connect(ao)
local vmix = vmix():connect(vo)
local avmix = {vmix, amix}

bpm(120)

local bank = getBank("superstratum_video-data")
local nodes = {}
local max_pop = 64
local next_spawn = 0
local spawn_every = 0.001
local die_every = 0.006
local next_die = die_every
local total_born = 0
local total_died = 0

function update()
    local bar = Time.bars

    while bar >= next_spawn and #nodes < max_pop do
        local i = (total_born % #bank) + 1
        local asset = bank[i]
        local node = sampler(asset.name):connect(avmix):mix(0.3):on()
        table.insert(nodes, {node = node, born = bar, name = asset.name})
        total_born = total_born + 1
        next_spawn = bar + spawn_every
    end

    while bar >= next_die and #nodes > 2 do
        local entry = table.remove(nodes, 1)
        entry.node:destroy()
        total_died = total_died + 1
        next_die = bar + die_every
    end

    if math.floor(bar) ~= math.floor(bar - (1/60) * (120/60)) then
        print("[STRESS] bar=" .. string.format("%.2f", bar)
            .. " pop=" .. #nodes
            .. " born=" .. total_born
            .. " died=" .. total_died)
    end
end
