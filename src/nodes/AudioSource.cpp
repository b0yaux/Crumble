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
    ControlSlot* positionSlot = nullptr;
    ControlSlot* loopSizeSlot = nullptr;

    // Unified trigger state: -1 = idle/rest, >= 0 = index into resolved paths
    std::atomic<int> pendingBufferIndex{-1};
    // NUMBER triggers (e.g., sampler("0 1 2")) — kept separate for now
    std::atomic<int> pendingTrigger{-1};
    // Sample-accurate playhead reset on any trigger
    std::atomic<bool> pendingRetrigger{false};
    TriggerMapPtr triggerMap;
    double lastTriggerBars = -1.0;

    AudioSourceProcessor() {
        playingSlot = getControlPtr(crumble::hashString("playing"));
        activeSlot = getControlPtr(crumble::hashString("active"));
        speedSlot = getControlPtr(crumble::hashString("speed"));
        gainSlot = getControlPtr(crumble::hashString("gain"));
        triggerSlot = getControlPtr(crumble::hashString("path"));
        loopSlot = getControlPtr(crumble::hashString("loop"));
        positionSlot = getControlPtr(crumble::hashString("position"));
        loopSizeSlot = getControlPtr(crumble::hashString("loopSize"));
    }

    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        if (evalSlot(activeSlot, cycle) < 0.5f) return;
        
        if (triggerSlot && triggerSlot->pattern) {
            if (lastTriggerBars < 0) {
                lastTriggerBars = std::floor(cycle);
            }
            
            if (std::abs(cycle - lastTriggerBars) > 0.5) {
                lastTriggerBars = std::floor(cycle);
            }

            double samplesPerBar = 1.0 / cycleStep;
            double endBars = cycle + (buffer.getNumFrames() * cycleStep);

            querySlot(triggerSlot, lastTriggerBars, endBars,
                      samplesPerBar, (int)buffer.getNumFrames(),
                      [this](const auto& e, int sampleOffset) {
                if (e.isRest) {
                    pendingBufferIndex.store(-1);
                    pendingTrigger.store(-1);
                } else if (e.ref) {
                    int idx = -1;
                    auto* map = triggerMap.get();
                    if (map) {
                        auto it = map->refToIndex.find(*e.ref);
                        if (it != map->refToIndex.end()) idx = it->second;
                    }
                    pendingBufferIndex.store(idx);
                    pendingRetrigger.store(true);
                    pendingTrigger.store(-1);
                } else {
                    pendingTrigger.store(static_cast<int>(std::floor(e.value)));
                    pendingRetrigger.store(true);
                    pendingBufferIndex.store(-1);
                }
            });
            lastTriggerBars = endBars;
        }
        
        size_t frames = buffer.getNumFrames();

        if (!data || totalSamples == 0) return;

        // Check for sample-accurate re-trigger (atomic flag set by pattern query)
        if (pendingRetrigger.load()) {
            double startPos = evalSlot(positionSlot, cycle) * (double)totalSamples;
            playhead.store(startPos);
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

            double regionStart = evalSlot(positionSlot, sampleCycle) * (double)totalSamples;
            double regionLen = std::max(1.0, evalSlot(loopSizeSlot, sampleCycle) * (double)totalSamples);
            if (regionStart + regionLen > (double)totalSamples) {
                regionStart = (double)totalSamples - regionLen;
            }
            double regionEnd = regionStart + regionLen;

            size_t frameIndex = (size_t)ph;
            if (frameIndex < totalSamples && channels > 0) {
                for (int c = 0; c < outCh; c++) {
                    buffer[i * outCh + c] += data[frameIndex * channels + (c % channels)] * curG;
                }
            }
            ph += spd;
            if (loopVal) {
                while (ph >= regionEnd) ph -= regionLen;
                while (ph < regionStart) ph += regionLen;
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
            // Start from the configured position, not 0. When a trigger
            // causes a file swap, LOAD_BUFFER may arrive in the next audio
            // callback — after the re-trigger was already consumed. Resetting
            // to 0 would leave the playhead stuck there until the next trigger.
            if (positionSlot && totalSamples > 0) {
                float posVal = positionSlot->value.load(std::memory_order_relaxed);
                playhead.store(posVal * (double)totalSamples);
            } else {
                playhead.store(0.0);
            }
        } else if (cmd.type == ProcessorCommand::RELEASE_BUFFER) {
            data        = nullptr;
            totalSamples = 0;
            channels    = 0;
            dataOwner.reset();
        } else if (cmd.type == ProcessorCommand::SET_TRIGGER_MAP) {
            triggerMap = cmd.triggerMap;
        }
    }

    int consumePendingBufferIndex() {
        return pendingBufferIndex.exchange(-1);
    }
    bool hasPendingTrigger() const { return pendingTrigger.load() >= 0; }
    int getPendingTrigger() const { return pendingTrigger.load(); }
    void clearPendingTrigger() { pendingTrigger.store(-1); }
};

} // namespace crumble

AudioSource::AudioSource() {
    type = "audio";
    parameters->add(path.set("path", ""));
    parameters->add(bank.set("bank", ""));
    parameters->add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters->add(playing.set("playing", true));
    parameters->add(position.set("position", 0.0, 0.0, 1.0));
    parameters->add(loop.set("loop", true));
    parameters->add(loopSize.set("loopSize", 1.0, 0.01, 1.0));

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
    // Dedup against last PARAMETER path (not pattern-loaded path).
    // The path pattern overrides loadedPath with bank:index values,
    // so comparing against loadedPath would fail dedup on reload.
    if (!p.empty() && p != lastParamPath) {
        lastParamPath = p;
        if (bank.get().empty()) bank.set(Node::validBank(p));
        load(p);
    }
}

void AudioSource::load(const std::string& audioPath) {
    std::string resolved = resolvePath(audioPath, "audio");
    if (resolved.empty()) {
        bank.set(Node::validBank(audioPath));
        pendingDecodePath.clear();
        return;
    }

    // Same file already loaded — skip redundant LOAD_BUFFER.
    // The audio-thread pendingRetrigger (set by pattern query) handles
    // playhead reset sample-accurately. Pushing LOAD_BUFFER again would
    // redundantly zero the playhead and waste command queue bandwidth.
    if (resolved == loadedResolvedPath) {
        pendingDecodePath.clear();
        return;
    }

    std::string ext = ofToLower(ofFilePath::getFileExt(resolved));
    if (ext == "mov" || ext == "hap" || ext == "mp4") {
        loadedPath = audioPath;
        if (loadEmbedded(resolved)) {
            loadedResolvedPath = resolved;
            pendingDecodePath.clear();
        } else {
            pendingDecodePath = audioPath;
        }
        return;
    }

    auto audioFile = getAudioAsset(resolved);
    if (audioFile && audioFile->loaded()) {
        loadedPath = audioPath;
        loadedResolvedPath = resolved;
        pendingDecodePath.clear();
        crumble::ProcessorCommand cmd;
        cmd.type         = crumble::ProcessorCommand::LOAD_BUFFER;
        cmd.nodeId       = nodeId;
        cmd.audioProcessor = audioProcessor;
        cmd.audioData    = audioFile->data();
        cmd.dataOwner    = audioFile;
        cmd.totalSamples = audioFile->length();
        cmd.channels     = audioFile->channels();
        pushCommand(cmd);
        ofLogVerbose("AudioSource") << "Loaded: " << audioPath;
    } else {
        // nullptr means async decode is in progress — retry next frame
        pendingDecodePath = audioPath;
    }
}

bool AudioSource::loadEmbedded(const std::string& videoPath) {
    if (!audioProcessor) return false;

    auto* cache = getCache();
    if (!cache) return false;

    int targetRate = getSampleRate();
    auto decoded = cache->getEmbeddedAudio(videoPath, targetRate);
    if (!decoded || decoded->numFrames == 0) return false;

    crumble::ProcessorCommand cmd;
    cmd.type           = crumble::ProcessorCommand::LOAD_BUFFER;
    cmd.nodeId         = nodeId;
    cmd.audioProcessor = audioProcessor;
    cmd.audioData      = decoded->data.data();
    cmd.dataOwner      = std::static_pointer_cast<void>(decoded);
    cmd.totalSamples   = decoded->numFrames;
    cmd.channels       = decoded->channels;
    pushCommand(cmd);
    
    ofLogVerbose("AudioSource") << "Embedded audio loaded: " << decoded->numFrames << " frames";
    return true;
}

crumble::AudioProcessor* AudioSource::createAudioProcessor() {
    return new crumble::AudioSourceProcessor();
}

void AudioSource::processAudio(ofSoundBuffer& buffer, int index) {
}

void AudioSource::onParameterChanged(const std::string& paramName) {
    Node::onParameterChanged(paramName);

    // Fast-Sync Path: explicit command dispatch for critical playback controls.
    // This bypasses generic parameter group iteration to guarantee that shadow 
    // processors adoption of static values is immediate and robust.
    if (paramName == "speed" || paramName == "loop" || paramName == "playing") {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::SET_PARAM;
        cmd.nodeId = nodeId;
        cmd.paramHash = crumble::hashString(paramName.c_str());
        cmd.audioProcessor = audioProcessor;
        
        float vVal = 0;
        if (paramName == "speed") vVal = speed.get();
        else if (paramName == "loop") vVal = loop.get() ? 1.0f : 0.0f;
        else if (paramName == "playing") vVal = playing.get() ? 1.0f : 0.0f;
        
        cmd.value = vVal;
        pushCommand(cmd);
    }
}

void AudioSource::update(float dt) {

    if (!audioProcessor) return;

    auto* pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    if (!pProc) return;

    if (!pendingDecodePath.empty()) {
        load(pendingDecodePath);
        if (!pendingDecodePath.empty()) {
            if (++pendingDecodeRetries > MAX_DECODE_RETRIES) {
                ofLogError("AudioSource") << "Decode timeout: " << pendingDecodePath;
                pendingDecodePath.clear();
                pendingDecodeRetries = 0;
            }
        } else {
            pendingDecodeRetries = 0;
        }
        return;
    }

    int idx = pProc->consumePendingBufferIndex();
    if (idx >= 0 && idx < (int)resolvedPaths.size()) {
        load(resolvedPaths[idx]);
    } else if (pProc->hasPendingTrigger()) {
        int trigIdx = pProc->getPendingTrigger();
        pProc->clearPendingTrigger();
        std::string b = bank.get();
        if (!b.empty()) {
            load(b + ":" + std::to_string(trigIdx));
        }
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
    auto* pProc = static_cast<crumble::AudioSourceProcessor*>(audioProcessor);
    if (!pProc || pProc->totalSamples == 0) return 0.0;
    return pProc->playhead.load() / (double)pProc->totalSamples;
}

void AudioSource::setRelativePosition(double pct) {
    // Deprecated: position is now a per-sample ControlSlot on the audio thread.
    // Retained for the deprecated C++ AVSampler caller.
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

void AudioSource::setResolvedPaths(const std::vector<std::string>& paths) {
    resolvedPaths = paths;
}
