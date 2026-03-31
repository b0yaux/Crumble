#include "ofMain.h"
#include "AudioSource.h"
#include "../core/NodeProcessor.h"
#include "../core/ProcessorCommand.h"
#include "../core/AudioCache.h"

namespace crumble {

class AudioSourceProcessor : public AudioProcessor {
public:
    ControlSlot* speedSlot = nullptr;
    ControlSlot* playingSlot = nullptr;
    ControlSlot* loopSlot = nullptr;
    ControlSlot* activeSlot = nullptr;
    ControlSlot* gainSlot = nullptr;
    ControlSlot* triggerSlot = nullptr;

    std::atomic<int> pendingTrigger{-1};
    std::atomic<bool> pendingRest{false};
    char pendingTriggerPathBuf[512] = {0};
    std::atomic<bool> hasPendingTriggerPath{false};
    std::mutex pendingPathMutex;
    std::atomic<bool> muted{false};
    std::atomic<bool> pendingRetrigger{false};  // Sample-accurate re-trigger flag
    double lastTriggerBars = -1.0;

    AudioSourceProcessor() {
        playingSlot = getControlPtr(crumble::hashString("playing"));
        activeSlot = getControlPtr(crumble::hashString("active"));
        loopSlot = getControlPtr(crumble::hashString("loop"));
        speedSlot = getControlPtr(crumble::hashString("speed"));
        gainSlot = getControlPtr(crumble::hashString("gain"));
        triggerSlot = getControlPtr(crumble::hashString("path"));
    }

    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        if (evalSlot(activeSlot, cycle) < 0.5f) return;
        
        if (triggerSlot && triggerSlot->pattern) {
            if (lastTriggerBars < 0) lastTriggerBars = cycle;
            
            if (std::abs(cycle - lastTriggerBars) > 0.5) lastTriggerBars = cycle;

            double samplesPerBar = 1.0 / cycleStep;
            double endBars = cycle + (buffer.getNumFrames() * cycleStep);

            querySlot(triggerSlot, lastTriggerBars, endBars,
                      samplesPerBar, (int)buffer.getNumFrames(),
                      [this](const auto& e, int sampleOffset) {
                if (e.isRest) {
                    pendingRest.store(true);
                    pendingTrigger.store(-1);
                    std::lock_guard<std::mutex> lock(pendingPathMutex);
                    hasPendingTriggerPath.store(false);
                } else if (e.ref) {
                    pendingRest.store(false);
                    std::string ref = *(e.ref);
                    std::lock_guard<std::mutex> lock(pendingPathMutex);
                    size_t len = std::min(ref.length(), sizeof(pendingTriggerPathBuf) - 1);
                    memcpy(pendingTriggerPathBuf, ref.c_str(), len);
                    pendingTriggerPathBuf[len] = '\0';
                    hasPendingTriggerPath.store(true);
                    pendingTrigger.store(-1);
                } else {
                    pendingRest.store(false);
                    pendingTrigger.store(static_cast<int>(std::floor(e.value)));
                    pendingRetrigger.store(true);  // Set flag for immediate sample-accurate reset
                    std::lock_guard<std::mutex> lock(pendingPathMutex);
                    hasPendingTriggerPath.store(false);
                }
            });
            lastTriggerBars = endBars;
        }
        
        bool isMuted = muted.load();
        size_t frames = buffer.getNumFrames();

        if (isMuted || !data || totalSamples == 0) return;

        // Check for sample-accurate re-trigger (atomic flag set by pattern query)
        if (pendingRetrigger.load()) {
            playhead.store(0.0);
            pendingRetrigger.store(false);
        }

        bool isPlaying = evalSlot(playingSlot, cycle) > 0.5f;
        if (!isPlaying) return;

        bool loopVal = evalSlot(loopSlot, cycle) > 0.5f;
        double ph = playhead.load();
        int outCh = buffer.getNumChannels();

        for (size_t i = 0; i < frames; i++) {
            double sampleCycle = cycle + i * cycleStep;
            float spd = evalSlot(speedSlot, sampleCycle);
            float curG = evalSlot(gainSlot, sampleCycle);

            size_t frameIndex = (size_t)ph;
            if (frameIndex < totalSamples && channels > 0) {
                for (int c = 0; c < outCh; c++) {
                    buffer[i * outCh + c] += data[frameIndex * channels + (c % channels)] * curG;
                }
            }
            ph += spd;
            if (loopVal) {
                while (ph >= (double)totalSamples) ph -= totalSamples;
                while (ph < 0) ph += totalSamples;
            } else if (ph >= (double)totalSamples || ph < 0) {
                ph = ofClamp(ph, 0.0, (double)totalSamples);
            }
        }
        playhead.store(ph);
    }

    void handleCommand(const ProcessorCommand& cmd) override {
        // Handle core parameter and pattern updates via base class
        AudioProcessor::handleCommand(cmd);

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
        } else if (cmd.type == ProcessorCommand::SET_RELATIVE_POS) {
            if (totalSamples > 0) {
                playhead.store(cmd.value * totalSamples);
            }
        }
    }

    void setMuted(bool m) { muted.store(m); }
    bool hasPendingTrigger() const { return pendingTrigger.load() >= 0; }
    int getPendingTrigger() const { return pendingTrigger.load(); }
    void clearPendingTrigger() { pendingTrigger.store(-1); }
    bool hasPendingRest() const { return pendingRest.load(); }
    void clearPendingRest() { pendingRest.store(false); }

    bool hasPendingPath() const { return hasPendingTriggerPath.load(); }
    std::string getPendingPath() {
        std::lock_guard<std::mutex> lock(pendingPathMutex);
        std::string p(pendingTriggerPathBuf);
        hasPendingTriggerPath.store(false);
        return p;
    }
};

} // namespace crumble

AudioSource::AudioSource() {
    type = "audio";
    parameters->add(path.set("path", ""));
    parameters->add(bank.set("bank", ""));
    parameters->add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters->add(loop.set("loop", true));
    parameters->add(playing.set("playing", true));
    parameters->add(position.set("position", 0.0, 0.0, 1.0));

    path.addListener(this, &AudioSource::onPathChanged);
}

AudioSource::~AudioSource() {
    path.removeListener(this, &AudioSource::onPathChanged);
    if (audioProcessor) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::RELEASE_BUFFER;
        cmd.audioProcessor = audioProcessor;
        pushCommand(cmd);
    }
}

void AudioSource::onPathChanged(std::string& p) {
    // Dedup: skip reload if path unchanged. Prevents redundant loads when
    // proxy fanout forwards the same static value to multiple children.
    if (!p.empty() && p != loadedPath) {
        if (bank.get().empty()) bank.set(Node::extractBank(p));
        load(p);
    }
}

void AudioSource::load(const std::string& audioPath) {
    std::string resolved = resolvePath(audioPath, "audio");
    if (resolved.empty()) return;

    std::string ext = ofToLower(ofFilePath::getFileExt(resolved));
    if (ext == "mov" || ext == "hap" || ext == "mp4") {
        loadEmbedded(resolved);
        return;
    }

    auto audioFile = getAudioAsset(resolved);
    if (audioFile && audioFile->loaded()) {
        loadedPath = audioPath;
        crumble::ProcessorCommand cmd;
        cmd.type         = crumble::ProcessorCommand::LOAD_BUFFER;
        cmd.nodeId       = nodeId;
        cmd.audioProcessor = audioProcessor;
        cmd.audioData    = audioFile->data();
        cmd.dataOwner    = audioFile;
        cmd.totalSamples = audioFile->length();
        cmd.channels     = audioFile->channels();
        pushCommand(cmd);
        ofLogNotice("AudioSource") << "Loaded: " << audioPath;
    } else {
        ofLogError("AudioSource") << "Failed to load: " << audioPath;
    }
}

void AudioSource::loadEmbedded(const std::string& videoPath) {
    if (!audioProcessor) return;

    auto* cache = getCache();
    if (!cache) return;

    // Decode embedded audio via custom ffmpeg logic in AudioCache
    int targetRate = getSampleRate();
    auto decoded = cache->getEmbeddedAudio(videoPath, targetRate);
    if (!decoded || decoded->numFrames == 0) return;

    // Push buffer pointer and metadata to the audio thread
    crumble::ProcessorCommand cmd;
    cmd.type           = crumble::ProcessorCommand::LOAD_BUFFER;
    cmd.nodeId         = nodeId;
    cmd.audioProcessor = audioProcessor;
    cmd.audioData      = decoded->data.data();
    cmd.dataOwner      = std::static_pointer_cast<void>(decoded);
    cmd.totalSamples   = decoded->numFrames;
    cmd.channels       = decoded->channels;
    pushCommand(cmd);
    
    ofLogNotice("AudioSource") << "Embedded audio loaded: " << decoded->numFrames << " frames";
}

crumble::AudioProcessor* AudioSource::createAudioProcessor() {
    return new crumble::AudioSourceProcessor();
}

void AudioSource::processAudio(ofSoundBuffer& buffer, int index) {
}

void AudioSource::onParameterChanged(const std::string& paramName) {
    Node::onParameterChanged(paramName);
}

void AudioSource::update(float dt) {
    if (!audioProcessor) return;

    auto* pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    if (!pProc) return;

    if (pProc->hasPendingPath()) {
        std::string resolvedPath = pProc->getPendingPath();
        // Dedup: skip reload if already loaded. Pattern-triggered events
        // re-fire on every cycle; without this, same-path triggers cause
        // redundant I/O stalls (T3.2 perf finding).
        if (!resolvedPath.empty() && resolvedPath != loadedPath) {
            load(resolvedPath);
            setRelativePosition(position.get());
            setMuted(false);
        }
    } else if (pProc->hasPendingTrigger()) {
        int idx = pProc->getPendingTrigger();
        pProc->clearPendingTrigger();
        std::string b = bank.get();
        if (!b.empty()) {
            load(b + ":" + std::to_string(idx));
        }
        setRelativePosition(position.get());
        setMuted(false);
    }
}

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

double AudioSource::getRelativePosition() const {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    if (!pProc || pProc->totalSamples == 0) return 0.0;
    return pProc->playhead.load() / (double)pProc->totalSamples;
}

void AudioSource::setRelativePosition(double pct) {
    crumble::ProcessorCommand cmd;
    cmd.type = crumble::ProcessorCommand::SET_RELATIVE_POS;
    cmd.nodeId = nodeId;
    cmd.audioProcessor = audioProcessor;
    cmd.value = pct;
    pushCommand(cmd);
}

void AudioSource::setMuted(bool muted) {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    if (pProc) pProc->setMuted(muted);
}

bool AudioSource::getMuted() const {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    return pProc ? pProc->muted.load() : false;
}

bool AudioSource::hasPendingTrigger() const {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    return pProc ? pProc->hasPendingTrigger() : false;
}

int AudioSource::getPendingTrigger() const {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    return pProc ? pProc->getPendingTrigger() : -1;
}

void AudioSource::clearPendingTrigger() {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    if (pProc) pProc->clearPendingTrigger();
}

bool AudioSource::hasPendingRest() const {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    return pProc ? pProc->hasPendingRest() : false;
}

void AudioSource::clearPendingRest() {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    if (pProc) pProc->clearPendingRest();
}

bool AudioSource::hasPendingPath() const {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    return pProc ? pProc->hasPendingPath() : false;
}

std::string AudioSource::getPendingPath() {
    auto pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    return pProc ? pProc->getPendingPath() : "";
}
