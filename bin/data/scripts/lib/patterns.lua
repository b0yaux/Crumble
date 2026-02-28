-- lib/patterns.lua
-- Functional helpers for time-based sequencing in Crumble

local P = {}

-- Returns a sine wave oscillating between 0 and 1 over 'frequency' cycles per beat
function P.osc(cycle, freq)
    freq = freq or 1.0
    return (math.sin(cycle * freq * math.pi * 2) + 1.0) * 0.5
end

-- Returns a ramp from 0 to 1 over 'frequency' cycles per beat
function P.ramp(cycle, freq)
    freq = freq or 1.0
    local phase = cycle * freq
    return phase - math.floor(phase)
end

-- Mini-Tidal string sequencer
-- Example: P.step(cycle, "1 0 0.5 0.2")
function P.step(cycle, patternStr)
    -- Simple cache to avoid parsing strings every frame
    P._cache = P._cache or {}
    
    local steps = P._cache[patternStr]
    if not steps then
        steps = {}
        for w in patternStr:gmatch("%S+") do
            local num = tonumber(w)
            table.insert(steps, num or w) -- Store number if possible, else string
        end
        P._cache[patternStr] = steps
    end
    
    if #steps == 0 then return 0 end
    
    -- Normalize cycle to 0..1 phase
    local phase = cycle - math.floor(cycle)
    
    -- Find which step we are on
    local stepIndex = math.floor(phase * #steps) + 1
    return steps[stepIndex]
end

return P
