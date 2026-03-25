-- stdlib.lua
-- Crumble Standard Library for Declarative Node Construction

-- Core Audio / Video Generators & Sources
function sampler(name, params) return addNode("AVSampler", name, params) end
function audio(name, params) return addNode("AudioFileSource", name, params) end
function video(name, params) return addNode("VideoFileSource", name, params) end

-- Mixers
function audiomix(name, params) return addNode("AudioMixer", name, params) end
function videomix(name, params) return addNode("VideoMixer", name, params) end

-- Outputs
function audioout(name, params) return addNode("SpeakersOutput", name, params) end
function videoout(name, params) return addNode("ScreenOutput", name, params) end

-- Subgraphs
function inlet(name, params) return addNode("Inlet", name, params) end
function outlet(name, params) return addNode("Outlet", name, params) end
function sub(name, params) return addNode("Graph", name, params) end
