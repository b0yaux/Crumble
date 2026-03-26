-- prelude.lua
-- Crumble Standard Library for Declarative Node Construction
-- This is the single source of truth for the user-facing DSL.

-- =============================================================================
-- CORE NODE FACTORIES
-- =============================================================================

function sampler(n, p) return addNode("sampler", n, p, "sampler") end
function audio(n, p)   return addNode("audio", n, p, "audio") end
function video(n, p)   return addNode("video", n, p, "video") end

function audiomix(n, p) return addNode("audiomix", n, p, "audiomix") end
function videomix(n, p) return addNode("videomix", n, p, "videomix") end

function audioout(n, p) return addNode("audioout", n, p, "audioout") end
function videoout(n, p) return addNode("videoout", n, p, "videoout") end

function inlet(n, p)  return addNode("inlet", n, p, "inlet") end
function outlet(n, p) return addNode("outlet", n, p, "outlet") end
function graph(n, p)  return addNode("graph", n, p, "graph") end

-- Aliases for convenience
function amix(n, p) return addNode("audiomix", n, p, "amix") end
function vmix(n, p) return addNode("videomix", n, p, "vmix") end
function aout(n, p) return addNode("audioout", n, p, "aout") end
function vout(n, p) return addNode("videoout", n, p, "vout") end
function s(n, p)    return addNode("sampler",  n, p, "s")    end

-- =============================================================================
-- MODULATION PATTERNS
-- Patterns are sampled at audio/video rate and can be chained/composed.
-- =============================================================================

-- Oscillators
function sine(f) return makeGen({type="osc", val=f or 1}) end
function saw(f)  return makeGen({type="ramp", val=f or 1}) end
function noise(f, s) return makeGen({type="noise", val=f or 1, s=s or 0}) end
function seq(str) return makeGen({type="seq", val=str}) end

-- Hydra-compatible aliases
function osc(f) return sine(f) end
function ramp(f) return saw(f) end

-- Utilities
function rand(s) 
    local x = (s or 0) * 1103515245 + 12345
    x = (math.floor(x / 65536) % 32768) / 32768.0
    return x
end

-- =============================================================================
-- EXTERNAL HARDWARE INPUT
-- Real-time modulation from MIDI, OSC, or Gamepad.
-- All values are normalized to 0.0-1.0 range.
-- =============================================================================

-- MIDI Control Change (knobs, faders, pedals)
-- Usage: :gain(midi(74, 1))  -- CC74 on channel 1
function midi(cc, chan) return makeGen({type="midi", cc=cc or 1, chan=chan or 1}) end

-- MIDI Note velocity (pad strike intensity)
-- Usage: :opacity(midinote(36, 10))  -- Note 36 on channel 10
function midinote(n, chan) return makeGen({type="midinote", note=n or 60, chan=chan or 1}) end

-- MIDI Polyphonic Aftertouch (per-key pressure, rare controllers only)
-- Usage: :filter(miditouch(36, 1))
function miditouch(n, chan) return makeGen({type="miditouch", note=n or 60, chan=chan or 1}) end

-- MIDI Channel Aftertouch (global keyboard pressure)
-- Usage: :speed(channeltouch(1):scale(0.5, 2.0))
function channeltouch(chan) return makeGen({type="channeltouch", chan=chan or 1}) end

-- OSC Input (listens on port 8000)
-- Usage: :gain(oscin("/filterCutoff"))
function oscin(path) return makeGen({type="oscin", path=path or ""}) end

-- =============================================================================
-- GAMEPAD INPUT
-- SDL2 GameController API with semantic naming
-- =============================================================================

-- Button name constants (Xbox/DualSense layout)
GPAD = {
    A = 0, B = 1, X = 2, Y = 3,
    LB = 4, RB = 5, BACK = 6, START = 7,
    GUIDE = 8, LS = 9, RS = 10,
    UP = 11, DOWN = 12, LEFT = 13, RIGHT = 14
}

-- Axis name constants
AXIS = {
    LX = 0, LY = 1,       -- Left stick X/Y
    RX = 2, RY = 3,       -- Right stick X/Y  
    LT = 4, RT = 5        -- Left/Right triggers (0..1, not -1..1)
}

-- Gamepad button by semantic name or index
-- Usage: s1:gain(gpad("a")) or s1:gain(gpad(GPAD.A))
function gpad(nameOrId)
    local id = type(nameOrId) == "string" and (GPAD[nameOrId:upper()] or 0) or (nameOrId or 0)
    return makeGen({type="gamepadbutton", id=id})
end

-- Gamepad axis by semantic name or index
-- Usage: s1:speed(gax("ly"):scale(0.5, 2.0))
function gax(nameOrId)
    local id = type(nameOrId) == "string" and (AXIS[nameOrId:upper()] or 0) or (nameOrId or 0)
    return makeGen({type="gamepadaxis", id=id})
end

-- Legacy low-level API (kept for backwards compatibility)
function gamepadbutton(id) return makeGen({type="gamepadbutton", id=id or 0}) end
function gamepadaxis(id) return makeGen({type="gamepadaxis", id=id or 0}) end

-- =============================================================================
-- PATTERN COMPOSITION
-- Patterns can be combined and transformed:
-- =============================================================================
-- p:scale(min, max)           -- Remap output to range
-- p:shift(amount)             -- Phase offset
-- p + q                       -- Addition
-- p * q                       -- Multiplication
-- p:snap(quantum)             -- Quantize to grid
-- =============================================================================

