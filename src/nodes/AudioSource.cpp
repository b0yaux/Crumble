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
    ControlSlot* gainSlot = nullptr;
    ControlSlot* triggerSlot = nullptr;
    ControlSlot* startSlot = nullptr;
    ControlSlot* endSlot = nullptr;

    static constexpr int MAX_TRIGGERS_PER_BUFFER = 64;

    struct PendingTrigger {
        int sampleOffset;
        int dataIndex;
    };

    TriggerMapPtr triggerMap;
    double lastTriggerBars = -1.0;

    AudioSourceProcessor() {
        playingSlot = getControlPtr(crumble::hashString("playing"));
        speedSlot = getControlPtr(crumble::hashString("speed"));
        gainSlot = getControlPtr(crumble::hashString("gain"));
        triggerSlot = getControlPtr(crumble::hashString("path"));
        loopSlot = getControlPtr(crumble::hashString("loop"));
        startSlot = getControlPtr(crumble::hashString("start"));
        endSlot = getControlPtr(crumble::hashString("end"));
    }

    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        size_t frames = buffer.getNumFrames();

        // Collect trigger events for this buffer into a sorted stack buffer.
        // querySlot computes per-sample offsets; we consume them in the DSP loop.
        PendingTrigger triggerBuf[MAX_TRIGGERS_PER_BUFFER];
        int numTriggers = 0;

        if (triggerSlot && triggerSlot->pattern) {
            if (lastTriggerBars < 0) {
                lastTriggerBars = std::floor(cycle);
            }

            if (std::abs(cycle - lastTriggerBars) > 0.5) {
                lastTriggerBars = std::floor(cycle);
            }

            double samplesPerBar = 1.0 / cycleStep;
            double endBars = cycle + (frames * cycleStep);

            querySlot(triggerSlot, lastTriggerBars, endBars,
                      samplesPerBar, (int)frames,
                      [this, &triggerBuf, &numTriggers](const auto& e, int sampleOffset) {
                if (e.isRest) return;
                if (numTriggers >= MAX_TRIGGERS_PER_BUFFER) return;

                int idx = -1;
                auto* map = triggerMap.get();
                if (!map) return;
                if (e.ref) {
                    auto it = map->refToIndex.find(*e.ref);
                    if (it != map->refToIndex.end()) idx = it->second;
                } else {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::floor(e.value)));
                    auto it = map->refToIndex.find(buf);
                    if (it != map->refToIndex.end()) idx = it->second;
                }

                if (idx >= 0 && idx < (int)map->audioData.size()) {
                    const auto& entry = map->audioData[idx];
                    if (entry.data && entry.totalSamples > 0) {
                        triggerBuf[numTriggers++] = {sampleOffset, idx};
                    }
                }
            });
            lastTriggerBars = endBars;
        }

        // Consume triggers before the null guard so the first trigger
        // swaps in data from the TriggerMap even when data starts null.
        int triggerIdx = 0;
        if (numTriggers > 0) {
            int firstOffset = triggerBuf[0].sampleOffset;
            double triggerCycle = cycle + firstOffset * cycleStep;
            while (triggerIdx < numTriggers && triggerBuf[triggerIdx].sampleOffset == firstOffset) {
                const auto& entry = triggerMap->audioData[triggerBuf[triggerIdx].dataIndex];
                dataOwner   = entry.owner;
                data        = entry.data;
                totalSamples = entry.totalSamples;
                channels    = entry.channels;
                double startPos = evalSlot(startSlot, triggerCycle) * (double)totalSamples;
                playhead.store(startPos);
                triggerIdx++;
            }
        }

        if (!data || totalSamples == 0) return;

        bool isPlaying = evalSlot(playingSlot, cycle) > 0.5f;
        if (!isPlaying) return;

        bool loopVal = evalSlot(loopSlot, cycle) > 0.5f;
        double ph = playhead.load();
        int outCh = buffer.getNumChannels();

        for (size_t i = 0; i < frames; i++) {
            // Apply any triggers scheduled at this sample
            while (triggerIdx < numTriggers && triggerBuf[triggerIdx].sampleOffset == (int)i) {
                const auto& entry = triggerMap->audioData[triggerBuf[triggerIdx].dataIndex];
                dataOwner   = entry.owner;
                data        = entry.data;
                totalSamples = entry.totalSamples;
                channels    = entry.channels;
                double startPos = evalSlot(startSlot, cycle + i * cycleStep) * (double)totalSamples;
                ph = startPos;
                triggerIdx++;
            }

            double sampleCycle = cycle + i * cycleStep;
            float spd = evalSlot(speedSlot, sampleCycle);
            float curG = evalSlot(gainSlot, sampleCycle);

            double regionStart = evalSlot(startSlot, sampleCycle) * (double)totalSamples;
            double regionEnd   = evalSlot(endSlot, sampleCycle) * (double)totalSamples;
            regionEnd = std::min(regionEnd, (double)totalSamples);
            if (regionEnd <= regionStart) regionEnd = regionStart + 1.0;
            double regionLen = regionEnd - regionStart;

            // Linear interpolation: reads two consecutive samples and
            // blends by the fractional part of the playhead. This eliminates
            // clicks at loop boundaries and improves quality for speed changes.
            size_t frameIndex = (size_t)ph;
            double frac = ph - (double)frameIndex;
            if (frameIndex < totalSamples && channels > 0) {
                size_t nextIndex = frameIndex + 1;
                if (loopVal) {
                    // Wrap nextIndex into the loop region so interpolation
                    // smoothly crosses the boundary instead of reading junk.
                    if (nextIndex >= (size_t)regionEnd) {
                        nextIndex = (size_t)regionStart;
                    }
                } else {
                    // One-shot: clamp to the last valid sample.
                    if (nextIndex >= totalSamples) {
                        nextIndex = totalSamples - 1;
                    }
                }
                for (int c = 0; c < outCh; c++) {
                    float s0 = data[frameIndex * channels + (c % channels)];
                    float s1 = data[nextIndex  * channels + (c % channels)];
                    buffer[i * outCh + c] += (s0 + (s1 - s0) * (float)frac) * curG;
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

    void handleCommand(ProcessorCommand& cmd) override {
        AudioProcessor::handleCommand(cmd);

        if (cmd.type == ProcessorCommand::LOAD_BUFFER) {
            dataOwner   = cmd.dataOwner;
            data        = cmd.audioData;
            totalSamples = cmd.totalSamples;
            channels    = cmd.channels;
            if (startSlot && totalSamples > 0) {
                float posVal = startSlot->value.load(std::memory_order_relaxed);
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
            // Displace the old TriggerMap so its destructor runs on the main thread,
            // not inside the real-time audio callback. Same pattern as SET_PATTERN.
            cmd.displacedTriggerMap = std::move(triggerMap);
            triggerMap = cmd.triggerMap;
        }
    }
};

} // namespace crumble

AudioSource::AudioSource() {
    type = "audio";
    parameters->add(path.set("path", ""));
    parameters->add(bank.set("bank", ""));
    parameters->add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters->add(playing.set("playing", true));
    parameters->add(start.set("start", 0.0, 0.0, 1.0));
    parameters->add(loop.set("loop", true));
    parameters->add(end.set("end", 1.0, 0.0, 1.0));

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
    // Trigger-driven swaps happen on the audio thread via TriggerMap.
    // This path is for initial loads (before TriggerMap exists) and
    // non-trigger path changes.
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

/**
 * Extract string refs from a pattern by querying one cycle.
 * The pattern's query() returns Event<float>s with optional ref fields.
 * Wrapper patterns (Density, Scale, etc.) preserve refs through transformations,
 * so querying the outermost pattern captures all refs in the composition.
 * Stateful patterns (Accum, Smooth, Toggle) collapse events and never produce refs.
 */
static std::vector<std::string> collectPatternRefs(const std::shared_ptr<Pattern<float>>& pat) {
    if (!pat) return {};
    auto events = pat->query(0.0, 1.0);
    std::unordered_set<std::string> unique;
    for (const auto& e : events) {
        if (e.ref && !e.isRest) unique.insert(*e.ref);
    }
    return {unique.begin(), unique.end()};
}

void AudioSource::prepareTrigger(const std::string& name, std::shared_ptr<Pattern<float>> pat) {
    if (name != "path" || !pat) return;
    auto refs = collectPatternRefs(pat);
    if (refs.empty()) return;
    std::string bankName = bank.get();
    if (!buildTriggerMap(refs, bankName)) {
        setPendingTriggerBuild(std::move(refs), bankName);
    }
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

void AudioSource::setPendingTriggerBuild(std::vector<std::string> refs, const std::string& bankName) {
    pendingTriggerBuild.refs = std::move(refs);
    pendingTriggerBuild.bankName = bankName;
}

bool AudioSource::buildTriggerMap(const std::vector<std::string>& refs, const std::string& bankName) {
    if (!audioProcessor) return false;

    auto* cache = getCache();
    if (!cache) return false;

    auto triggerMap = std::make_shared<crumble::TriggerMap>();
    int index = 0;
    for (const auto& ref : refs) {
        bool isNumeric = false;
        try {
            size_t pos;
            std::stof(ref, &pos);
            isNumeric = (pos == ref.size());
        } catch (...) {}

        std::string resolved;
        if (isNumeric && !bankName.empty()) {
            resolved = resolvePath(bankName + ":" + ref, "audio");
        } else {
            resolved = resolvePath(ref, "audio");
        }
        if (resolved.empty()) continue;

        triggerMap->refToIndex[ref] = index;

        crumble::AudioTriggerEntry entry;
        std::string ext = resolved.substr(resolved.find_last_of('.') + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == "mov" || ext == "hap" || ext == "mp4") {
            auto decoded = cache->getEmbeddedAudio(resolved, getSampleRate());
            if (decoded) {
                entry.data = decoded->data.data();
                entry.totalSamples = decoded->numFrames;
                entry.channels = decoded->channels;
                entry.owner = decoded;
            }
        } else {
            auto audioFile = cache->getAudio(resolved);
            if (audioFile && audioFile->loaded()) {
                entry.data = audioFile->data();
                entry.totalSamples = audioFile->length();
                entry.channels = (int)audioFile->channels();
                entry.owner = audioFile;
            }
        }
        triggerMap->audioData.push_back(entry);
        index++;
    }

    for (const auto& e : triggerMap->audioData) {
        if (!e.data || e.totalSamples == 0) return false;
    }

    if (!triggerMap->audioData.empty()) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::SET_TRIGGER_MAP;
        cmd.audioProcessor = audioProcessor;
        cmd.triggerMap = triggerMap;
        pushCommand(cmd);
    }
    return true;
}

void AudioSource::update(float dt) {

    if (!audioProcessor) return;

    if (!pendingDecodePath.empty()) {
        load(pendingDecodePath);
    }

    if (!pendingTriggerBuild.refs.empty()) {
        if (buildTriggerMap(pendingTriggerBuild.refs, pendingTriggerBuild.bankName)) {
            pendingTriggerBuild.refs.clear();
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
}
