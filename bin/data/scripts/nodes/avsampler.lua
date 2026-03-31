-- nodes/avsampler.lua
-- Lua sub-graph replacement for C++ AVSampler.
-- Parent usage: graph("name", {script="nodes/avsampler.lua"})
-- API identical to sampler(): path, speed, mix(), blend(), position, on()/off()

local a = audio("")
local v = video("")

a:outlet(0)
v:outlet(0)

v.clockMode = 1

expose(a, "speed", "path", "gain", "loop", "position")
expose(v, "speed", "path", "opacity", "blend")
-- position on video is slaved to audio playhead in update() — not exposed to parent.
-- "active" is NOT exposed — Graph::onParameterChanged("active")
-- propagates to all children automatically.

function update()
    v.position = playhead(a)
end
