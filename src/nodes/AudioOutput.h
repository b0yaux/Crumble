#pragma once
#include "../core/Node.h"

class AudioOutput : public Node {
public:
    AudioOutput();
    
    crumble::AudioProcessor* createAudioProcessor() override;
    void setupProcessor() override;
};
