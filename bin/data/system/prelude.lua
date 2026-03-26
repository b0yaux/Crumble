-- prelude.lua
-- Crumble Standard Library for Declarative Node Construction

-- Core Audio / Video Generators & Sources
function sampler(n, p) return addNode("sampler", n, p, "sampler") end
function audio(n, p)   return addNode("audio", n, p, "audio") end
function video(n, p)   return addNode("video", n, p, "video") end

-- Mixers
function audiomix(n, p) return addNode("audiomix", n, p, "audiomix") end
function videomix(n, p) return addNode("videomix", n, p, "videomix") end

-- Outputs
function audioout(n, p) return addNode("audioout", n, p, "audioout") end
function videoout(n, p) return addNode("videoout", n, p, "videoout") end

-- Subgraphs / Modular Nodes
function inlet(n, p)  return addNode("inlet", n, p, "inlet") end
function outlet(n, p) return addNode("outlet", n, p, "outlet") end
function graph(n, p)  return addNode("graph", n, p, "graph") end

-- Mini-Notation Aliases (Performer Shorthands)
function amix(n, p) return addNode("audiomix", n, p, "amix") end
function vmix(n, p) return addNode("videomix", n, p, "vmix") end
function aout(n, p) return addNode("audioout", n, p, "aout") end
function vout(n, p) return addNode("videoout", n, p, "vout") end
function s(n, p)    return addNode("sampler",  n, p, "s")    end
