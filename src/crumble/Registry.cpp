#include "Registry.h"
#include "../core/Graph.h"
#include "../nodes/video/VideoMixer.h"
#include "../nodes/video/VideoFileSource.h"
#include "../nodes/video/ScreenOutput.h"
#include "../nodes/audio/SpeakersOutput.h"
#include "../nodes/audio/AudioFileSource.h"
#include "../nodes/audio/AudioMixer.h"
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
    s.registerNodeType("Inlet", []() {
        return std::make_unique<Inlet>();
    });
    s.registerNodeType("Outlet", []() {
        return std::make_unique<Outlet>();
    });
}

} // namespace crumble
