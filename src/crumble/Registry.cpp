#include "Registry.h"
#include "../nodes/video/VideoMixer.h"
#include "../nodes/video/VideoFileSource.h"
#include "../nodes/video/ScreenOutput.h"

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
}

} // namespace crumble
