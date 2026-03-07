#include "ofMain.h"
#include "AudioFileSource.h"
#include "../core/AudioCommand.h"
#include "../core/NodeProcessor.h"
#include "../core/Session.h"

namespace crumble {

class AudioFileProcessor : public AudioProcessor {
public:
    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        // Use name-based lookup — no more index confusion!
        if (!data || totalSamples == 0 || getParam("playing") < 0.5f || getParam("active") < 0.5f) return;

        bool loop = getParam("loop") > 0.5f;

        double currentPlayhead = playhead.load();

        for (size_t i = 0; i < buffer.getNumFrames(); i++) {
            double sampleCycle = cycle + i * cycleStep;

            // Evaluate speed and volume by name — falls back to scalar if no pattern installed
            float speed = evalPattern("speed", sampleCycle);
            float gain  = evalPattern("volume", sampleCycle);

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
    
    void handleCommand(const AudioCommand& cmd) override {
        if (cmd.type == AudioCommand::LOAD_BUFFER) {
            // Store the lifetime anchor first so the data pointer is never
            // valid without the underlying buffer being kept alive.
            dataOwner   = cmd.dataOwner;
            data        = cmd.audioData;
            totalSamples = cmd.totalSamples;
            channels    = cmd.channels;
            playhead.store(0.0);
        } else if (cmd.type == AudioCommand::RELEASE_BUFFER) {
            // Zero the raw pointer before releasing the owner so there is
            // never a window where data is non-null but the buffer is freed.
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
    // Keeps the ofxAudioFile buffer alive for as long as this processor
    // holds a reference, regardless of what the UI thread does with the
    // originating AudioFileSource.
    std::shared_ptr<void> dataOwner;
};

} // namespace crumble

AudioFileSource::~AudioFileSource() {
    // Send RELEASE_BUFFER before ~Node() sends REMOVE_NODE.
    // This lets the AudioFileProcessor zero its data pointer and release its
    // dataOwner reference in the correct order: the raw pointer is cleared
    // first, then the owning shared_ptr, then the processor is unregistered.
    if (audioProcessor) {
        crumble::AudioCommand cmd;
        cmd.type = crumble::AudioCommand::RELEASE_BUFFER;
        pushCommand(cmd);
    }
}

AudioFileSource::AudioFileSource() {
    type = "AudioFileSource";
    parameters->add(path.set("path", ""));
    parameters->add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters->add(loop.set("loop", true));
    parameters->add(playing.set("playing", true));
    path.addListener(this, &AudioFileSource::onPathChanged);
    // NOTE: setupProcessor() is NOT called here.
    // When used standalone, Graph::createNode() -> ptr->setupProcessor() handles it.
    // When used as AVSampler::audioSource, AVSampler::createProcessor() sets the
    // processor directly to avoid a double ADD_NODE / ghost processor leak.
}

crumble::AudioProcessor* AudioFileSource::createAudioProcessor() {
    return new crumble::AudioFileProcessor();
}

void AudioFileSource::processAudio(ofSoundBuffer& buffer, int index) {
    // DSP is handled by AudioFileProcessor
}

std::string AudioFileSource::getDisplayName() const {
    if (path.get().empty()) return "Empty Audio";
    return ofFilePath::getFileName(path.get());
}

ofJson AudioFileSource::serialize() const {
    ofJson j;
    ofSerialize(j, *parameters);
    return j;
}

void AudioFileSource::deserialize(const ofJson& json) {
    ofDeserialize(json, *parameters);
}

void AudioFileSource::onPathChanged(std::string& p) {
    if (!p.empty() && p != loadedPath) {
        load(p);
    }
}

void AudioFileSource::load(const std::string& p) {
    if (!sharedLoader) sharedLoader = std::make_shared<ofxAudioFile>();
    
    sharedLoader->load(p);
    
    if (sharedLoader->loaded()) {
        loadedPath = p;
        
        crumble::AudioCommand cmd;
        cmd.type = crumble::AudioCommand::LOAD_BUFFER;
        cmd.nodeId = nodeId;
        cmd.audioProcessor = audioProcessor;
        cmd.audioData  = sharedLoader->data();
        cmd.dataOwner  = sharedLoader;   // keeps buffer alive in the processor
        cmd.totalSamples = sharedLoader->length();
        cmd.channels   = sharedLoader->channels();
        pushCommand(cmd);
        
        ofLogNotice("AudioFileSource") << "Loaded: " << p << " (" << sharedLoader->length() << " samples)";
    } else {
        ofLogError("AudioFileSource") << "Failed to load: " << p;
    }
}

void AudioFileSource::onParameterChanged(const std::string& paramName) {
    // Node::onParameterChanged handles the generic SET_PARAM command
    // via the slotMap — no need for hardcoded slot indices here.
    Node::onParameterChanged(paramName);
}

double AudioFileSource::getRelativePosition() const {
    auto pProc = static_cast<crumble::AudioFileProcessor*>(audioProcessor);
    if (!pProc || !sharedLoader || sharedLoader->length() == 0) return 0.0;
    return pProc->playhead.load() / (double)sharedLoader->length();
}

void AudioFileSource::setRelativePosition(double pct) {
    auto pProc = static_cast<crumble::AudioFileProcessor*>(audioProcessor);
    if (!pProc || !sharedLoader || sharedLoader->length() == 0) return;
    pProc->playhead.store(ofClamp(pct, 0.0, 1.0) * (double)sharedLoader->length());
}
