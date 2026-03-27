#include "AVSampler.h"
#include "ofMain.h"
#include "../core/NodeProcessor.h"
#include "../core/Graph.h"
#include "../core/Session.h"

// Audio processor that pulls from embedded video audio (ofxHapPlayer)
class VideoEmbedAudioProcessor : public crumble::AudioProcessor {
public:
    ofBaseSoundOutput* audioOutput = nullptr;
    crumble::ControlSlot* gainSlot = nullptr;
    crumble::ControlSlot* activeSlot = nullptr;
    std::atomic<bool> muted{false};
    bool loggedOnce = false;
    
    VideoEmbedAudioProcessor(ofBaseSoundOutput* audioOut) : audioOutput(audioOut) {
        gainSlot = getControlPtr(crumble::hashString("gain"));
        activeSlot = getControlPtr(crumble::hashString("active"));
        ofLogNotice("VideoEmbedAudioProcessor") << "Created with audioOutput=" << (audioOut ? "valid" : "null");
    }
    
    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        buffer.set(0.0f);  // Start with silence
        
        if (!audioOutput) {
            if (!loggedOnce) {
                ofLogWarning("VideoEmbedAudioProcessor") << "No audio output!";
                loggedOnce = true;
            }
            return;
        }
        
        if (muted.load()) {
            // Muted for REST - return silence
            return;
        }
        
        if (evalSlot(activeSlot, cycle) < 0.5f) return;
        
        // Pull audio from ofxHapPlayer's AudioOutput
        audioOutput->audioOut(buffer);
        
        // Apply gain
        float g = evalSlot(gainSlot, cycle);
        if (g < 0.999f || g > 1.001f) {
            buffer *= g;
        }
    }
    
    void setMuted(bool m) { 
        muted.store(m); 
        if (m && !loggedOnce) {
            ofLogNotice("VideoEmbedAudioProcessor") << "Muted for REST";
        }
    }
};

AVSampler::AVSampler() {
    type = "sampler";
    
    // Add parameters
    parameters->add(path.set("path", ""));
    parameters->add(audioPath.set("audioPath", ""));
    parameters->add(videoPath.set("videoPath", ""));
    parameters->add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters->add(loop.set("loop", true));
    parameters->add(playing.set("playing", true));
    parameters->add(position.set("position", 0.0, 0.0, 1.0));
    
    lastSyncedAudioPos = -1.0;
    useEmbeddedAudio = false;
}

AVSampler::~AVSampler() {
    if (audioProcessor) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::RELEASE_BUFFER;
        pushCommand(cmd);
    }
    audioSource.audioProcessor = nullptr;
}

crumble::AudioProcessor* AVSampler::createAudioProcessor() {
    // If video has embedded audio, pull from VideoSource's player
    if (useEmbeddedAudio && videoSource.hasEmbeddedAudio()) {
        ofBaseSoundOutput* audioOut = videoSource.getPlayer().getAudioOutput();
        if (audioOut) {
            ofLogNotice("AVSampler") << "Creating audio processor from embedded video audio";
            embeddedAudioProcessor = new VideoEmbedAudioProcessor(audioOut);
            return embeddedAudioProcessor;
        }
    }
    
    // Otherwise use standalone AudioSource
    crumble::AudioProcessor* proc = audioSource.createAudioProcessor();
    if (proc) {
        audioSource.audioProcessor = proc;
        proc->nodeId = audioSource.nodeId;

        for (int i = 0; i < (int)audioSource.parameters->size(); i++) {
            auto& p = audioSource.parameters->get(i);
            float val = 0;
            bool supported = false;

            if (p.type() == typeid(ofParameter<float>).name()) {
                val = p.cast<float>().get();
                supported = true;
            } else if (p.type() == typeid(ofParameter<bool>).name()) {
                val = p.cast<bool>().get() ? 1.0f : 0.0f;
                supported = true;
            } else if (p.type() == typeid(ofParameter<int>).name()) {
                val = (float)p.cast<int>().get();
                supported = true;
            }

            if (supported) {
                if (auto* slot = proc->getControlPtr(crumble::hashString(p.getName().c_str()))) {
                    slot->value.store(val, std::memory_order_relaxed);
                }
            }
        }
    }
    return proc;
}

crumble::VideoProcessor* AVSampler::createVideoProcessor() {
    crumble::VideoProcessor* proc = videoSource.createVideoProcessor();
    if (proc) {
        proc->nodeId = videoSource.nodeId;

        for (int i = 0; i < (int)videoSource.parameters->size(); i++) {
            auto& p = videoSource.parameters->get(i);
            float val = 0;
            bool supported = false;

            if (p.type() == typeid(ofParameter<float>).name()) {
                val = p.cast<float>().get();
                supported = true;
            } else if (p.type() == typeid(ofParameter<bool>).name()) {
                val = p.cast<bool>().get() ? 1.0f : 0.0f;
                supported = true;
            } else if (p.type() == typeid(ofParameter<int>).name()) {
                val = (float)p.cast<int>().get();
                supported = true;
            }

            if (supported) {
                if (auto* slot = proc->getControlPtr(crumble::hashString(p.getName().c_str()))) {
                    slot->value.store(val, std::memory_order_relaxed);
                }
            }
        }
        
        for (int i = 0; i < (int)parameters->size(); i++) {
            auto& p = parameters->get(i);
            std::string name = p.getName();
            if (name == "opacity" || name == "gain" || name == "active") {
                float val = 0;
                bool supported = false;
                
                if (p.type() == typeid(ofParameter<float>).name()) {
                    val = p.cast<float>().get();
                    supported = true;
                } else if (p.type() == typeid(ofParameter<bool>).name()) {
                    val = p.cast<bool>().get() ? 1.0f : 0.0f;
                    supported = true;
                }
                
                if (supported) {
                    if (auto* slot = proc->getControlPtr(crumble::hashString(name.c_str()))) {
                        slot->value.store(val, std::memory_order_relaxed);
                    }
                }
            }
        }
    }
    return proc;
}

void AVSampler::prepare(const Context& ctx) {
    if (!active->get()) return;

    if (graph && (audioSource.graph != graph || videoSource.graph != graph)) {
        audioSource.graph = graph;
        videoSource.graph = graph;
        videoSource.setClockMode(VideoSource::INTERNAL);
    }
    
    Node::prepare(ctx);
    audioSource.prepare(ctx);
    if (ctx.frames <= 1) {
        videoSource.prepare(ctx);
    }
}

void AVSampler::update(float dt) {
    if (!active->get()) return;

    // Query index pattern for events (Tidal-style)
    if (indexPattern && graph) {
        double currentCycle = graph->getTransport().cycle;
        
        // Query events since last cycle
        if (currentCycle > lastQueryCycle) {
            auto events = indexPattern->query(lastQueryCycle < 0 ? 0 : lastQueryCycle, currentCycle);
            if (!events.empty()) {
                ofLogNotice("AVSampler") << "Pattern query: cycle=" << currentCycle << " events=" << events.size();
            }
            for (const auto& e : events) {
                if (e.isRest) {
                    ofLogNotice("AVSampler") << "Pattern event: REST at cycle " << e.onset;
                    silenceSample();
                } else {
                    ofLogNotice("AVSampler") << "Pattern event: TRIGGER " << e.value << " at cycle " << e.onset;
                    int idx = static_cast<int>(std::floor(e.value));
                    idx = std::max(0, idx);
                    triggerSample(idx);
                }
            }
        } else if (lastQueryCycle < 0) {
            // First evaluation - trigger at cycle 0
            ofLogNotice("AVSampler") << "Pattern first evaluation at cycle 0";
            auto events = indexPattern->query(0, 0.0001);
            for (const auto& e : events) {
                if (!e.isRest) {
                    int idx = static_cast<int>(std::floor(e.value));
                    idx = std::max(0, idx);
                    triggerSample(idx);
                }
            }
        }
        lastQueryCycle = currentCycle;
    }

    if (!audioPath.get().empty() && !videoPath.get().empty()) {
        double audioPos = audioSource.getRelativePosition();
        const double DRIFT_TOLERANCE = 0.05;
        if (std::abs(audioPos - lastSyncedAudioPos) > DRIFT_TOLERANCE) {
            videoSource.setPosition((float)audioPos);
            lastSyncedAudioPos = audioPos;
        }

        isInternalChange = true;
        position.set((float)audioPos);
        isInternalChange = false;
    }
    
    videoSource.update(dt);
    masterPlayhead = audioSource.getRelativePosition();
}

void AVSampler::setIndexPattern(std::shared_ptr<Pattern<float>> pat) {
    indexPattern = pat;
    lastQueryCycle = -1.0;  // Reset for fresh evaluation
}

void AVSampler::triggerSample(int index) {
    // Build path from bank name and index
    std::string newPath;
    if (!bankName.empty()) {
        // Bank mode: bank:index
        newPath = bankName + ":" + std::to_string(index);
    } else if (!path.get().empty()) {
        // Single-file mode: keep existing path, just restart
        // Path was set directly (e.g., "travaux" or "/path/to/file")
        newPath = path.get();
    } else {
        // No path set yet, use index as path
        newPath = std::to_string(index);
    }
    
    ofLogNotice("AVSampler") << "triggerSample(" << index << ") -> path: " << newPath << " (bankName: " << bankName << ")";
    
    // Only reload if path changed
    if (path.get() != newPath) {
        path.set(newPath);
        onParameterChanged("path");
    }
    
    // Restart playback
    if (useEmbeddedAudio) {
        // Embedded audio: VideoSource handles both video and audio
        videoSource.setPosition(0.0f);
        if (embeddedAudioProcessor) {
            embeddedAudioProcessor->setMuted(false);
        }
    } else {
        // Separate files: restart both
        audioSource.setRelativePosition(0.0);
        videoSource.setPosition(0.0f);
    }
    
    // Ensure playing
    if (!playing.get()) {
        playing.set(true);
        onParameterChanged("playing");
    }
}

void AVSampler::silenceSample() {
    // Mute audio - handle embedded vs separate
    if (useEmbeddedAudio && embeddedAudioProcessor) {
        // For embedded audio, mute the VideoEmbedAudioProcessor
        embeddedAudioProcessor->setMuted(true);
    } else {
        audioSource.gain->set(0.0f);
    }
}

void AVSampler::modulate(const std::string& paramName, std::shared_ptr<Pattern<float>> pat) {
    if (paramName == "n" || paramName == "index") {
        // Extract bank name from pattern if present
        if (auto* seqPat = dynamic_cast<patterns::Seq*>(pat.get())) {
            std::string sig = seqPat->getSignature();
            size_t secondColon = sig.find(':', sig.find(':') + 1);
            if (secondColon != std::string::npos) {
                std::string b = sig.substr(secondColon + 1);
                if (!b.empty()) {
                    bankName = b;
                }
            }
        }
        setIndexPattern(pat);
    } else {
        // Pass through to base class for regular modulation
        Node::modulate(paramName, pat);
    }
}

void AVSampler::setupProcessor() {
    audioSource.nodeId = nodeId;
    videoSource.nodeId = nodeId;
    audioSource.graph = graph;
    videoSource.graph = graph;
    Node::setupProcessor();
    audioSource.audioProcessor = audioProcessor;
    videoSource.videoProcessor = videoProcessor;
}

void AVSampler::processAudio(ofSoundBuffer& buffer, int index) {}

ofTexture* AVSampler::processVideo(int index) {
    if (index == 0) return videoSource.getVideoOutput(index);
    return nullptr;
}

std::string AVSampler::getDisplayName() const {
    std::string display = "AVSampler";
    if (!path.get().empty()) display += " [" + path.get() + "]";
    else if (!videoPath.get().empty()) display += " [" + ofFilePath::getFileName(videoPath.get()) + "]";
    return display;
}

void AVSampler::onParameterChanged(const std::string& paramName) {
    if (paramName == "path") {
        std::string pathVal = path.get();
        ofLogNotice("AVSampler") << "path changed to: " << pathVal;
        if (pathVal.empty()) return;
        std::string vid = resolvePath(pathVal, "video");
        std::string aud = resolvePath(pathVal, "audio");
        ofLogNotice("AVSampler") << "Resolved video: " << vid << " audio: " << aud;
        
        // If video and audio are the same file, use embedded audio from video
        if (aud == vid && !vid.empty()) {
            ofLogNotice("AVSampler") << "Using embedded audio from video file";
            useEmbeddedAudio = true;
            aud = "";  // Don't load separate audio
        } else {
            useEmbeddedAudio = false;
        }
        
        if (aud != audioPath.get()) {
            audioPath.set(aud);
            onParameterChanged("audioPath");
        }
        if (vid != videoPath.get()) {
            videoPath.set(vid);
            onParameterChanged("videoPath");
        }
        return;
    }
    if (paramName == "audioPath") {
        if (!audioPath.get().empty() && audioPath.get() != loadedAudioPath) {
            ofLogNotice("AVSampler") << "Loading audio: " << audioPath.get();
            audioSource.load(audioPath.get());
            loadedAudioPath = audioPath.get();
            masterPlayhead = 0.0;
        }
        return;
    }
    if (paramName == "videoPath") {
        if (!videoPath.get().empty() && videoPath.get() != loadedVideoPath) {
            ofLogNotice("AVSampler") << "Loading video: " << videoPath.get();
            videoSource.load(videoPath.get());
            loadedVideoPath = videoPath.get();
            masterPlayhead = 0.0;
        }
        return;
    }

    if (paramName == "speed") {
        audioSource.speed.set(speed.get());
        videoSource.speed.set(speed.get());
        auto pat = getPattern("speed");
        audioSource.modulate("speed", pat);
        audioSource.onParameterChanged("speed");
        videoSource.modulate("speed", pat);
        videoSource.onParameterChanged("speed");
    } else if (paramName == "gain") {
        audioSource.gain->set(gain->get());
        auto pat = getPattern("gain");
        audioSource.modulate("gain", pat);
        audioSource.onParameterChanged("gain");
    } 
    else if (paramName == "opacity") {
        auto pat = getPattern("opacity");
        videoSource.modulate("opacity", pat);
        videoSource.onParameterChanged("opacity");
    } else if (paramName == "active") {
        bool isActive = active->get();
        audioSource.active->set(isActive);
        videoSource.active->set(isActive);
        if (!isActive) {
            audioSource.playing.set(false);
        } else {
            audioSource.playing.set(playing.get());
        }
        audioSource.onParameterChanged("playing");
    }
    Node::onParameterChanged(paramName);
}

void AVSampler::setMasterPlayhead(double pos) { masterPlayhead = pos; }

ofJson AVSampler::serialize() const {
    ofJson j;
    ofSerialize(j, *parameters);
    return j;
}

void AVSampler::deserialize(const ofJson& json) {
    ofJson j = json;
    if (j.contains("AVSampler")) j = j["AVSampler"];
    else if (j.contains("group")) j = j["group"];
    if (j.contains("path")) path.set(getSafeJson<std::string>(j, "path", ""));
    if (j.contains("audioPath")) audioPath.set(getSafeJson<std::string>(j, "audioPath", ""));
    if (j.contains("videoPath")) videoPath.set(getSafeJson<std::string>(j, "videoPath", ""));
    ofDeserialize(j, *parameters);
}
