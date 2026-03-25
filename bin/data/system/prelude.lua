-- stdlib.lua
-- Crumble Standard Library for Declarative Node Construction

-- Core Audio / Video Generators & Sources
function Sampler(name, params) return addNode("AVSampler", name, params) end
function AudioFile(name, params) return addNode("AudioFileSource", name, params) end
function VideoFile(name, params) return addNode("VideoFileSource", name, params) end

-- Mixers
function AudioMixer(name, params) return addNode("AudioMixer", name, params) end
function VideoMixer(name, params) return addNode("VideoMixer", name, params) end

-- Outputs
function Speakers(name, params) return addNode("SpeakersOutput", name, params) end
function Screen(name, params) return addNode("ScreenOutput", name, params) end

-- Subgraphs
function Inlet(name, params) return addNode("Inlet", name, params) end
function Outlet(name, params) return addNode("Outlet", name, params) end
function Subgraph(name, params) return addNode("Graph", name, params) end
