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
    MULTIPLY = 2,   -- Multiply (darken)
    SCREEN = 3      -- Screen (commutative brighten)
}

-- =============================================================================
-- CORE NODE FACTORIES
-- =============================================================================

local function makeSampler(n, p, prefix)
    local params = (type(p) == "table") and p or {}
    local patternArg = nil
    local genTable = nil
    
    if type(n) == "table" and n._isGen then
        genTable = n
        n = nil
    elseif type(p) == "table" and p._isGen then
        genTable = p
        p = nil
    end
    
    if not genTable then
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
    end
    
    local deferredPath = params.path
    params.path = nil
    params.script = "scripts/nodes/avsampler.lua"

    local node = addNode("graph", n, params, prefix)
    if node then
        if genTable then
            node.path = genTable
        else
            if deferredPath then
                node.path = deferredPath
            end
            if patternArg then
                node.path = makeGen({type="seq", val=patternArg})
            end
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

function split(n, p) return addNode("split", n, p, "split") end

function delay(n, p) return addNode("delay", n, p, "delay") end

-- =============================================================================
-- FFT SPECTRAL ANALYSIS
-- :fft() enables spectral analysis on any node with an AudioProcessor.
-- After enabling, the node exposes :bass(), :mid(), :rms() etc. — gen tables
-- that return spectral data from the node's audio output.
-- These compose into any parameter like any gen table (modulation, viz, etc.).
-- =============================================================================

-- Internal: resolve Hz → bin index given a sample rate and FFT size.
-- Defaults to 44100 Hz / 2048 FFT if the processor isn't ready yet.
local function hzToBin(hz, fftNode)
    local sampleRate = 44100
    local fftSize = 2048
    if fftNode then
        local sr = _get(fftNode.id, "sampleRate")
        local sz = _get(fftNode.id, "fft")
        if sr and sr > 0 then sampleRate = sr end
        if sz and sz > 0 then fftSize = sz end
    end
    return math.floor(hz * fftSize / sampleRate)
end

local function fftMethods(node)
    local nId = node.id

    return {
        -- Single bin magnitude (raw bin index)
        bin = function(self, i) return makeGen({type="fft_bin", node=nId, bin=i}) end,

        -- Band RMS over bin range [lo, hi) (raw bin indices)
        bins = function(self, lo, hi) return makeGen({type="fft_band", node=nId, lo=lo, hi=hi}) end,

        -- Band RMS over Hz range
        band = function(self, lo, hi)
            return self:bins(hzToBin(lo, node), hzToBin(hi, node))
        end,

        -- Named bands (Hz-based)
        bass   = function(self) return self:band(20, 250) end,
        lowmid = function(self) return self:band(250, 500) end,
        mid    = function(self) return self:band(500, 2000) end,
        high   = function(self) return self:band(2000, 20000) end,

        -- Overall RMS
        rms = function(self) return makeGen({type="fft_rms", node=nId}) end,
    }
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

--- Drain pending MIDI note events (note-on and note-off).
--- Returns an array of tables: { on, note, velocity, channel, time }
--- @param chan number Channel filter (1-16), or nil for all channels.
--- @usage for _, e in ipairs(midievents(1)) do
---   if e.on then spawn_voice(e.note, e.velocity) end
--- end
function midievents(chan) return _midiEvents(chan or 0) end

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

-- PlayStation button names (standard SDL mapping)
-- cross=A, circle=B, square=X, triangle=Y
GPAD_PS = {
    CROSS = 0, CIRCLE = 1, SQUARE = 2, TRIANGLE = 3,
    SELECT = 4, PS = 5, OPTIONS = 6,
    L3 = 7, R3 = 8,
    L1 = 9, R1 = 10,
    UP = 11, DOWN = 12, LEFT = 13, RIGHT = 14
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
-- INPUT UTILITIES
--
-- Two layers for responding to input in Crumble:
--
-- 1. Pattern modulation — for continuous signal flow into node parameters.
--    Composed via the pattern chain: gpad("ly"):accum(-0.5, 0.5):scale(-3, 3)
--    Evaluated per-sample on the audio thread or per-frame on the video thread.
--
-- 2. Lua event primitives — for discrete decisions in update().
--    Used when input should trigger logic (randomize, create/destroy nodes,
--    branch, step counters) rather than modulate a parameter continuously.
--
-- All event primitives accept any source:
--   String          → gamepad name, auto-reads from Gamepad table
--   Number          → raw float value (nil treated as 0)
--   Gen table       → any gpad(), midi(), oscin() pattern source (read via C)
--
--   once("cross")           → gamepad shortcut
--   once(g.cross)           → Gamepad table value
--   once(gpad("cross"))     → gen table (unified input path)
--   once(midi(64, 1))       → MIDI note (gen table)
-- =============================================================================

-- _read(source) → float
-- Resolves any input source to its current numeric value.
-- Internal utility used by once/press/held.
function _read(source)
    if source == nil then return 0 end
    if type(source) == "number" then return source end
    if type(source) == "string" then return (Gamepad or {})[source] or 0 end
    if type(source) == "table" and source.type then
        return _readBinding(source) or 0
    end
    return 0
end

-- once(source) → bool
-- Fires once when the source goes above 0.5 (rising edge).
-- No repeat while held. Used for discrete actions.
--
--   if once("cross") then idx = math.random(0, total - 1) end
local _once = {}
function once(source)
    local v = _read(source)
    local s = _once[source]
    if not s then
        s = {held = false}
        _once[source] = s
    end
    if v > 0.5 and not s.held then
        s.held = true
        return true
    end
    if v <= 0.5 then
        s.held = false
    end
    return false
end

-- press(source, delay, rate) → bool
-- Fires immediately on press, then auto-repeats while held.
--
--   delay: frames before repeat starts (default: 18 = ~300ms at 60fps)
--   rate:  frames between repeats (default: 6 = ~100ms at 60fps)
--
--   if press("r1") then batch = batch + 1 end
--   if press("l1") then batch = batch - 1 end
local _press = {}
function press(source, delay, rate)
    delay = delay or 18
    rate = rate or 6
    local s = _press[source]
    if not s then
        s = {held = false, start = -999, last = -999, tick = 0}
        _press[source] = s
    end
    s.tick = s.tick + 1
    local v = _read(source)
    if v > 0.5 then
        if not s.held then
            s.held = true; s.start = s.tick; s.last = s.tick
            return true
        end
        if s.tick - s.start >= delay and s.tick - s.last >= rate then
            s.last = s.tick
            return true
        end
    else
        s.held = false
    end
    return false
end

-- held(source) → bool
-- True while the source is above 0.5. No edge detection.
-- For continuous hold actions (analog-style).
--
--   if held("up") then opacity = math.min(1, opacity + 0.02) end
function held(source)
    return _read(source) > 0.5
end

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
