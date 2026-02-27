#pragma once
#include "../core/Node.h"

/**
 * SpeakersOutput acts as the final audio sink.
 * It owns the ofSoundStream and pulls audio from the Graph.
 */
class SpeakersOutput : public Node {
public:
    SpeakersOutput();
    virtual ~SpeakersOutput();

    void setup(int rate = 44100, int size = 512);
    void onInputConnected(int& toInput) override;

    // Callback for ofSoundStream (OpenFrameworks system callback)
    void audioOut(ofSoundBuffer& buffer);

    // Crumble Graph API override
    void pullAudio(ofSoundBuffer& buffer, int index = 0) override;

protected:
    ofSoundStream soundStream;
    ofParameter<float> masterVolume;
    ofParameter<int> sampleRate;
    ofParameter<int> bufferSize;

    void initStream();
    void onSettingsChanged(int& val);
};
