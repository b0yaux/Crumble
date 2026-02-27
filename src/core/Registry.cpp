#include "Registry.h"
#include "Graph.h"
#include "../nodes/VideoMixer.h"
#include "../nodes/VideoFileSource.h"
#include "../nodes/ScreenOutput.h"
#include "../nodes/SpeakersOutput.h"
#include "../nodes/AudioFileSource.h"
#include "../nodes/AudioMixer.h"
#include "../nodes/AVSampler.h"
#include "../nodes/subgraph/Inlet.h"
#include "../nodes/subgraph/Outlet.h"

namespace crumble {

void registerNodes(Session& s) {
    s.registerNodeType("Graph", []() {
        return std::make_unique<Graph>();
    });
    s.registerNodeType("VideoMixer", []() {
        return std::make_unique<VideoMixer>();
    });
    s.registerNodeType("VideoFileSource", []() {
        return std::make_unique<VideoFileSource>();
    });
    s.registerNodeType("ScreenOutput", []() {
        return std::make_unique<ScreenOutput>();
    });
    s.registerNodeType("SpeakersOutput", []() {
        return std::make_unique<SpeakersOutput>();
    });
    s.registerNodeType("AudioFileSource", []() {
        return std::make_unique<AudioFileSource>();
    });
    s.registerNodeType("AudioMixer", []() {
        return std::make_unique<AudioMixer>();
    });
    s.registerNodeType("AVSampler", []() {
        return std::make_unique<AVSampler>();
    });
    s.registerNodeType("Inlet", []() {
        return std::make_unique<Inlet>();
    });
    s.registerNodeType("Outlet", []() {
        return std::make_unique<Outlet>();
    });
}

} // namespace crumble
