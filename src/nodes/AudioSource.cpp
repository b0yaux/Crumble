#include "ofMain.h"
#include "AudioSource.h"
#include "../core/ProcessorCommand.h"
#include "../core/NodeProcessor.h"

namespace crumble {

class AudioSourceProcessor : public AudioProcessor {
public:
    ControlSlot* playingSlot = nullptr;
    ControlSlot* activeSlot = nullptr;
    ControlSlot* loopSlot = nullptr;
    ControlSlot* speedSlot = nullptr;
    ControlSlot* gainSlot = nullptr;

    AudioSourceProcessor() {
        playingSlot = getControlPtr(crumble::hashString("playing"));
        activeSlot = getControlPtr(crumble::hashString("active"));
        loopSlot = getControlPtr(crumble::hashString("loop"));
        speedSlot = getControlPtr(crumble::hashString("speed"));
        gainSlot = getControlPtr(crumble::hashString("gain"));
    }

    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        if (!data || totalSamples == 0 || evalSlot(playingSlot, cycle) < 0.5f || evalSlot(activeSlot, cycle) < 0.5f) return;

        bool loop = evalSlot(loopSlot, cycle) > 0.5f;
        double currentPlayhead = playhead.load();

        for (size_t i = 0; i < buffer.getNumFrames(); i++) {
            double sampleCycle = cycle + i * cycleStep;
            float speed = evalSlot(speedSlot, sampleCycle);
            float gain  = evalSlot(gainSlot, sampleCycle);

            size_t frameIndex = (size_t)currentPlayhead;
            if (frameIndex < totalSamples) {
                for (int c = 0; c < buffer.getNumChannels(); c++) {
                    int sourceChannel = c % channels;
                    float sample = data[frameIndex * channels + sourceChannel];
                    buffer[i * buffer.getNumChannels() + c] += sample * gain;
                }
            }

            currentPlayhead += speed;

            if (loop) {
                while (currentPlayhead >= (double)totalSamples) currentPlayhead -= totalSamples;
                while (currentPlayhead < 0)                     currentPlayhead += totalSamples;
            } else if (currentPlayhead >= (double)totalSamples || currentPlayhead < 0) {
                currentPlayhead = ofClamp(currentPlayhead, 0.0, (double)totalSamples);
            }
        }
        playhead.store(currentPlayhead);
    }
    
    void handleCommand(const ProcessorCommand& cmd) override {
        if (cmd.type == ProcessorCommand::LOAD_BUFFER) {
            dataOwner   = cmd.dataOwner;
            data        = cmd.audioData;
            totalSamples = cmd.totalSamples;
            channels    = cmd.channels;
            playhead.store(0.0);
        } else if (cmd.type == ProcessorCommand::RELEASE_BUFFER) {
            data        = nullptr;
            totalSamples = 0;
            channels    = 0;
            dataOwner.reset();
        }
    }

    std::atomic<double> playhead{0.0};
    const float* data = nullptr;
    size_t totalSamples = 0;
    int channels = 0;

private:
    std::shared_ptr<void> dataOwner;
};

} // namespace crumble

AudioSource::~AudioSource() {
    if (audioProcessor) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::RELEASE_BUFFER;
        pushCommand(cmd);
    }
}

AudioSource::AudioSource() {
    type = "audio";
    parameters->add(path.set("path", ""));
    parameters->add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters->add(loop.set("loop", true));
    parameters->add(playing.set("playing", true));
    path.addListener(this, &AudioSource::onPathChanged);
}

crumble::AudioProcessor* AudioSource::createAudioProcessor() {
    return new crumble::AudioSourceProcessor();
}

void AudioSource::processAudio(ofSoundBuffer& buffer, int index) {}

std::string AudioSource::getDisplayName() const {
    if (path.get().empty()) return "Empty Audio";
    return ofFilePath::getFileName(path.get());
}

ofJson AudioSource::serialize() const {
    ofJson j;
    ofSerialize(j, *parameters);
    return j;
}

void AudioSource::deserialize(const ofJson& json) {
    ofDeserialize(json, *parameters);
}

void AudioSource::onPathChanged(std::string& p) {
    if (!p.empty() && p != loadedPath) {
        load(p);
    }
}

void AudioSource::load(const std::string& p) {
    sharedLoader = getAudioAsset(p);
    if (sharedLoader && sharedLoader->loaded()) {
        loadedPath = p;
        crumble::ProcessorCommand cmd;
        cmd.type         = crumble::ProcessorCommand::LOAD_BUFFER;
        cmd.nodeId       = nodeId;
        cmd.audioProcessor = audioProcessor;
        cmd.audioData    = sharedLoader->data();
        cmd.dataOwner    = sharedLoader;
        cmd.totalSamples = sharedLoader->length();
        cmd.channels     = sharedLoader->channels();
        pushCommand(cmd);
        ofLogNotice("AudioSource") << "Loaded: " << p;
    } else {
        ofLogError("AudioSource") << "Failed to load: " << p;
    }
}

void AudioSource::onParameterChanged(const std::string& paramName) {
    Node::onParameterChanged(paramName);
}

double AudioSource::getRelativePosition() const {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    if (!pProc || !sharedLoader || sharedLoader->length() == 0) return 0.0;
    return pProc->playhead.load() / (double)sharedLoader->length();
}

void AudioSource::setRelativePosition(double pct) {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    if (!pProc || !sharedLoader || sharedLoader->length() == 0) return;
    pProc->playhead.store(ofClamp(pct, 0.0, 1.0) * (double)sharedLoader->length());
}
