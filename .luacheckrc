std = "luajit"
cache = true

-- Crumble prelude globals (provided by C++ / prelude.lua)
globals = {
    -- Node factories
    "sampler", "audio", "video",
    "audiomix", "videomix", "audioout", "videoout",
    "inlet", "outlet", "graph",
    "s", "amix", "vmix", "aout", "vout",

    -- Pattern generators
    "osc", "sine", "ramp", "saw", "noise", "seq", "sp",
    "rand",

    -- Hardware input
    "midi", "midinote", "miditouch", "channeltouch", "oscin",
    "gpad", "gamepadbutton", "gamepadaxis",

    -- Sample aliases
    "alias", "aliases",

    -- Tempo
    "bpm", "cpm", "cps",

    -- Graph construction
    "addNode", "connect", "clear", "getBank",

    -- Core API (C bindings)
    "_addNode", "_connect", "_get", "_set", "_setGen",
    "_setActive", "_clear", "_resolve", "_setAlias", "_getBank",
    "_setTempo",

    -- Pattern internals
    "makeGen",

    -- Runtime globals
    "BLEND", "GPAD", "GPAD_PS", "AXIS",
    "Time",
    "update",

    -- Internal tables
    "_allNodes", "_autoIndices", "_autoNames",
}

-- Scripts use global by design (live-coding DSL)
allow_defined = true
unused = false
redefined = false

-- Ignore self/unused args in method definitions
ignore = {
    "212/self",
    "311/unused",
}

files = {
    bin/data/scripts/",
}
