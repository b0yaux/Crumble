-- prelude.lua
-- Crumble Standard Library for Declarative Node Construction
-- This is the single source of truth for the user-facing DSL.

-- =============================================================================
-- GLOBAL CONSTANTS
-- =============================================================================

-- Blend modes for video compositing (available immediately)
BLEND = {
    ALPHA = 0,      -- Normal alpha blending
    ADD = 1,        -- Additive (lighten)
    MULTIPLY = 2    -- Multiply (darken)
}

-- =============================================================================
-- CORE NODE FACTORIES
-- =============================================================================

local function makeSampler(n, p, prefix)
    local params = (type(p) == "table") and p or {}
    local patternArg = nil
    
    if type(p) == "string" then
        patternArg = p
        if not params.path then params.path = n end
    elseif type(n) == "string" and not params.path then
        local _, count = n:gsub("%S+", "")
        if count >= 2 then
            params.path = n:match("^([^%s]+)")
            patternArg = n
        else
            params.path = n
        end
    end
    
    local deferredPath = params.path
    params.path = nil
    params.script = "scripts/nodes/avsampler.lua"

    local node = addNode("graph", n, params, prefix)
    if node then
        if deferredPath then
            node.path = deferredPath
        end
        if patternArg then
            node.path = makeGen({type="seq", val=patternArg})
        end
    end
    return node
end

function sampler(n, p) return makeSampler(n, p, "sampler") end
function s(n, p)        return makeSampler(n, p, "s") end

function audio(n, p)
    local params = (type(p) == "table") and p or {}
    if type(n) == "string" then params.path = n end
    return addNode("audio", n, params, "audio")
end

function video(n, p)
    local params = (type(p) == "table") and p or {}
    if type(n) == "string" then params.path = n end
    return addNode("video", n, params, "video")
end

function audiomix(n, p) return addNode("audiomix", n, p, "audiomix") end
function videomix(n, p) return addNode("videomix", n, p, "videomix") end

function audioout(n, p) return addNode("audioout", n, p, "audioout") end
function videoout(n, p) return addNode("videoout", n, p, "videoout") end

function graph(n, p)  return addNode("graph", n, p, "graph") end

function amix(n, p) return addNode("audiomix", n, p, "amix") end
function vmix(n, p) return addNode("videomix", n, p, "vmix") end
function aout(n, p) return addNode("audioout", n, p, "aout") end
function vout(n, p) return addNode("videoout", n, p, "vout") end

-- =============================================================================
-- SAMPLE ALIASES
-- Define short names for samples, Strudel-style
-- =============================================================================

function alias(name, target)
    _setAlias(name, target)
end

function aliases(tbl)
    for name, target in pairs(tbl) do
        _setAlias(name, target)
    end
end

-- =============================================================================
-- MODULATION PATTERNS
-- Patterns are sampled at audio/video rate and can be chained/composed.
-- =============================================================================

-- Oscillators
function sine(f) return makeGen({type="osc", val=f or 1}) end
function saw(f)  return makeGen({type="ramp", val=f or 1}) end
function noise(f, s) return makeGen({type="noise", f=f or 1, s=s or 0}) end
function seq(str) return makeGen({type="seq", val=str}) end

-- Hydra-compatible aliases
function osc(f) return sine(f) end
function ramp(f) return saw(f) end

-- =============================================================================
-- EXTERNAL HARDWARE INPUT
-- Real-time modulation from MIDI, OSC, or Gamepad.
-- =============================================================================

function midi(cc, chan) return makeGen({type="midi", cc=cc or 1, chan=chan or 1}) end
function midinote(n, chan) return makeGen({type="midinote", note=n or 60, chan=chan or 1}) end
function miditouch(n, chan) return makeGen({type="miditouch", note=n or 60, chan=chan or 1}) end
function channeltouch(chan) return makeGen({type="channeltouch", chan=chan or 1}) end
function oscin(path) return makeGen({type="oscin", path=path or ""}) end

-- =============================================================================
-- GAMEPAD INPUT
-- Unified API supporting both Xbox and PlayStation controller naming
-- Auto-detects buttons vs axes for simplified usage
-- =============================================================================

-- Xbox button names
GPAD = {
    -- Face buttons
    A = 0, B = 1, X = 2, Y = 3,
    -- Shoulders
    LB = 4, RB = 5,
    -- Menu buttons
    BACK = 6, START = 7, GUIDE = 8,
    -- Stick clicks
    LS = 9, RS = 10,
    -- D-pad
    UP = 11, DOWN = 12, LEFT = 13, RIGHT = 14
}

-- PlayStation button names (same indices, different names)
-- cross=A, circle=B, square=X, triangle=Y
GPAD_PS = {
    CROSS = 0, CIRCLE = 1, SQUARE = 2, TRIANGLE = 3,
    L1 = 4, R1 = 5,
    SELECT = 6, OPTIONS = 7, PS = 8,
    L3 = 9, R3 = 10
}

-- Axis names (Xbox/PlayStation compatible)
AXIS = {
    LX = 0, LY = 1,       -- Left stick X/Y
    RX = 2, RY = 3,       -- Right stick X/Y  
    LT = 4, RT = 5        -- Left/Right triggers (0..1, not -1..1)
}

-- Unified gamepad input function
-- Accepts both Xbox and PlayStation names, auto-detects buttons vs axes
-- Usage:
--   gpad("a") or gpad("cross")     -> A/Cross button
--   gpad("l1") or gpad("lb")       -> Left bumper
--   gpad("ly") or gpad("lx")       -> Left stick Y/X axis
--   gpad("rx") or gpad("ry")       -> Right stick X/Y axis
--   gpad("lt") or gpad("rt")       -> Left/Right trigger
function gpad(nameOrId)
    if type(nameOrId) == "number" then
        return makeGen({type="gamepadbutton", id=nameOrId})
    end
    
    local name = nameOrId:upper()
    
    -- Check if it's an axis first
    if AXIS[name] then
        return makeGen({type="gamepadaxis", id=AXIS[name]})
    end
    
    -- Check Xbox button names
    if GPAD[name] then
        return makeGen({type="gamepadbutton", id=GPAD[name]})
    end
    
    -- Check PlayStation button names
    if GPAD_PS[name] then
        return makeGen({type="gamepadbutton", id=GPAD_PS[name]})
    end
    
    -- Unknown - return button 0
    return makeGen({type="gamepadbutton", id=0})
end

-- Low-level API (kept for backwards compatibility)
function gamepadbutton(id) return makeGen({type="gamepadbutton", id=id or 0}) end
function gamepadaxis(id) return makeGen({type="gamepadaxis", id=id or 0}) end

-- =============================================================================
-- PARAMETER PROXY FOR SUB-GRAPHS
-- expose() maps parent-facing parameter names to internal child node parameters.
-- Used inside sub-graph scripts (e.g. avsampler.lua) so the parent can set
-- parameters on children through the Graph node.
--
-- When the parent does s.speed = 1.5, the C++ layer looks up proxy targets
-- for "speed" on the Graph and forwards the value to each mapped child.
--
-- Forms (dispatched by first argument type):
--
--   String-first:
--     expose("speed", a)                                  -- same name, single child
--     expose("speed", a, "rate")                          -- name mapping
--     expose("intensity", {a, "level"}, {v, "brightness"}) -- custom compound param
--
--   Node-first (batch, one child, multiple same-name params):
--     expose(a, "speed", "path", "loop")
--
--   Tuple-based (batch, any combination):
--     expose({a, "speed"}, {v, "speed"})
--     expose({a, "speed"}, {a, "path"}, {v, "opacity"})
--
-- Multi-target (string-first) creates a custom parent-facing parameter that
-- fans out to multiple children with potentially different child param names:
--   s.intensity = 0.5  →  a.level = 0.5 AND v.brightness = 0.5
--
-- For prelude methods like mix() that fan out at the Lua level
-- (mix() calls _set(id, "gain", ...) + _set(id, "opacity", ...)), use
-- individual exposes with the child param names:
--   expose("gain", a) + expose("opacity", v)
-- =============================================================================

function expose(first, second, ...)

    -- Node-first batch: expose(node, "param1", "param2", ...)
    -- All params registered with same name on the given child.
    if type(first) == "table" and first.id then
        for _, paramName in ipairs({second, ...}) do
            if type(paramName) == "string" then
                _exposeParam(first.id, paramName, paramName)
            end
        end
        return
    end

    -- Tuple-based batch: expose({node, "param"}, {node, "param"}, ...)
    -- Each tuple is a same-name expose. Mix children and params freely.
    if type(first) == "table" and not first.id then
        for _, pair in ipairs({first, second, ...}) do
            if type(pair) == "table" and type(pair[1]) == "table" and pair[1].id
               and type(pair[2]) == "string" then
                _exposeParam(pair[1].id, pair[2], pair[2])
            end
        end
        return
    end

    -- String-first: single, mapped, or multi-target
    if type(first) == "string" then
        local parentParam = first
        if type(second) == "table" and second.id then
            -- expose("speed", a) or expose("speed", a, "rate")
            local childParam = ...
            _exposeParam(second.id, parentParam, childParam or parentParam)
        elseif type(second) == "table" then
            -- expose("intensity", {a, "level"}, {v, "brightness"})
            for _, pair in ipairs({second, ...}) do
                if type(pair) == "table" and pair.id and type(pair[2]) == "string" then
                    _exposeParam(pair.id, parentParam, pair[2])
                end
            end
        end
    end
end
