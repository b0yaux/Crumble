#include "Registry.h"
#include "../nodes/video/VideoMixer.h"
#include "../nodes/video/VideoFileSource.h"
#include "../nodes/video/ScreenOutput.h"
#include "../nodes/audio/SpeakersOutput.h"
#include "../nodes/audio/AudioFileSource.h"

namespace crumble {

void registerNodes(Session& s) {
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
}

} // namespace crumble
