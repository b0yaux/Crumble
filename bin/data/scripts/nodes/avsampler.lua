-- nodes/avsampler.lua
-- Lua sub-graph replacement for C++ AVSampler.
-- Parent usage: graph("name", {script="nodes/avsampler.lua"})
-- API identical to sampler(): path, speed, mix(), blend(), on()/off()

local a = audio("")
local v = video("")

a:outlet(0)
v:outlet(0)

v.clockMode = 1

expose(a, "speed", "path")
expose(v, "speed", "path")
expose("gain", a)
expose("opacity", v)
expose(v, "blend")
-- "active" is NOT exposed — Graph::onParameterChanged("active")
-- propagates to all children automatically.

function update()
    v.position = playhead(a)
end
