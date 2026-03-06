#include "ofMain.h"
#include "AudioFileSource.h"
#include "../core/AudioCommand.h"
#include "../core/NodeProcessor.h"
#include "../core/Session.h"

namespace crumble {

class AudioFileProcessor : public NodeProcessor {
public:
    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        // Use name-based slot lookup — never hardcoded integers
        if (!data || totalSamples == 0 || getParam("playing") < 0.5f) return;

        // Resolve slots once per block (cheap map lookup, not per-sample)
        int speedSlot  = getSlot("speed");
        int volSlot    = getSlot("volume");
        bool loop      = getParam("loop") > 0.5f;

        double currentPlayhead = playhead.load();

        for (size_t i = 0; i < buffer.getNumFrames(); i++) {
            double sampleCycle = cycle + i * cycleStep;

            // Evaluate speed and volume — falls back to scalar if no pattern installed
            float speed = evalPattern(speedSlot, sampleCycle);
            float gain  = evalPattern(volSlot,   sampleCycle);

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
            data = cmd.audioData;
            totalSamples = cmd.totalSamples;
            channels = cmd.channels;
            playhead.store(0.0);
        }
    }
    
    std::atomic<double> playhead{0.0};
    const float* data = nullptr;
    size_t totalSamples = 0;
    int channels = 0;
};

} // namespace crumble

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

crumble::NodeProcessor* AudioFileSource::createProcessor() {
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
        cmd.processor = processor;
        cmd.audioData = sharedLoader->data();
        cmd.totalSamples = sharedLoader->length();
        cmd.channels = sharedLoader->channels();
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
    auto pProc = static_cast<crumble::AudioFileProcessor*>(processor);
    if (!pProc || !sharedLoader || sharedLoader->length() == 0) return 0.0;
    return pProc->playhead.load() / (double)sharedLoader->length();
}

void AudioFileSource::setRelativePosition(double pct) {
    auto pProc = static_cast<crumble::AudioFileProcessor*>(processor);
    if (!pProc || !sharedLoader || sharedLoader->length() == 0) return;
    pProc->playhead.store(ofClamp(pct, 0.0, 1.0) * (double)sharedLoader->length());
}
