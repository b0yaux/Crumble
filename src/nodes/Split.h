#pragma once
#include "../core/Node.h"

class Split : public Node {
public:
    Split();

    crumble::AudioProcessor* createAudioProcessor() override;
    crumble::VideoProcessor* createVideoProcessor() override;

    std::string getDisplayName() const override { return "Split"; }
};
