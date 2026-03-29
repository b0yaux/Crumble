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
    if type(n) == "string" and not params.path then
        params.path = n
    end
    if type(p) == "string" then
        params.n = p
    end
    return addNode("sampler", n, params, prefix)
end

function sampler(n, p) return makeSampler(n, p, "sampler") end
function s(n, p)        return makeSampler(n, p, "s") end

function audio(n, p)   return addNode("audio", n, p, "audio") end
function video(n, p)   return addNode("video", n, p, "video") end

function audiomix(n, p) return addNode("audiomix", n, p, "audiomix") end
function videomix(n, p) return addNode("videomix", n, p, "videomix") end

function audioout(n, p) return addNode("audioout", n, p, "audioout") end
function videoout(n, p) return addNode("videoout", n, p, "videoout") end

function inlet(n, p)  return addNode("inlet", n, p, "inlet") end
function outlet(n, p) return addNode("outlet", n, p, "outlet") end
function graph(n, p)  return addNode("graph", n, p, "graph") end

function amix(n, p) return addNode("audiomix", n, p, "amix") end
function vmix(n, p) return addNode("videomix", n, p, "vmix") end
function aout(n, p) return addNode("audioout", n, p, "aout") end
function vout(n, p) return addNode("videoout", n, p, "vout") end

-- Pattern sampler: s("k s k") creates sampler with pattern of sample names
-- Each step resolves its alias at trigger time
function sp(pattern)
    if type(pattern) ~= "string" then return nil end
    
    -- Extract first token as initial path
    local firstToken = pattern:match("^([^%s]+)")
    if not firstToken then return nil end
    
    -- Create sampler with first token as path, rest as pattern
    return s(firstToken, pattern)
end

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
-- PATTERN COMPOSITION
-- =============================================================================
-- p:scale(min, max), p:shift(amount), p + q, p * q, p:snap(quantum)
