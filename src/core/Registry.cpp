#include "Registry.h"
#include "Graph.h"
#include "../nodes/VideoMixer.h"
#include "../nodes/VideoSource.h"
#include "../nodes/VideoOutput.h"
#include "../nodes/AudioOutput.h"
#include "../nodes/AudioSource.h"
#include "../nodes/AudioMixer.h"
#include "../nodes/Split.h"
#include "../nodes/Delay.h"
#include "../nodes/FFT.h"

namespace crumble {

void registerNodes(Session& s) {
    s.registerNodeType("graph", []() {
        return std::make_unique<Graph>();
    });
    s.registerNodeType("videomix", []() {
        return std::make_unique<VideoMixer>();
    });
    s.registerNodeType("video", []() {
        return std::make_unique<VideoSource>();
    });
    s.registerNodeType("videoout", []() {
        return std::make_unique<VideoOutput>();
    });
    s.registerNodeType("audioout", []() {
        return std::make_unique<AudioOutput>();
    });
    s.registerNodeType("audio", []() {
        return std::make_unique<AudioSource>();
    });
    s.registerNodeType("audiomix", []() {
        return std::make_unique<AudioMixer>();
    });
    s.registerNodeType("split", []() {
        return std::make_unique<Split>();
    });
    s.registerNodeType("delay", []() {
        return std::make_unique<Delay>();
    });
    s.registerNodeType("fft", []() {
        return std::make_unique<FFT>();
    });
}

} // namespace crumble
