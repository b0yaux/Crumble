#pragma once
#include "../core/Node.h"

/**
 * SpeakersOutput acts as the final audio sink.
 * It does not own the audio thread anymore (Session does).
 * It simply acts as a marker/router node that Session pulls from.
 */
class SpeakersOutput : public Node {
public:
    SpeakersOutput();
    virtual ~SpeakersOutput();

    // Crumble Graph API override
    void pullAudio(ofSoundBuffer& buffer, int index = 0) override;

protected:
    ofParameter<float> masterVolume;
};
