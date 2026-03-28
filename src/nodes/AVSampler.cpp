#include "AVSampler.h"
#include "ofMain.h"
#include "../core/NodeProcessor.h"
#include "../core/Graph.h"
#include "../core/Session.h"
#include "../core/Patterns.h"
#include "../core/AssetRegistry.h"

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
    parameters->add(triggerPosition.set("triggerPosition", 0.0, 0.0, 1.0));
}

AVSampler::~AVSampler() {
    audioSource.audioProcessor = nullptr;
}

crumble::AudioProcessor* AVSampler::createAudioProcessor() {
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
        // EXTERNAL mode: video is slaved to audio playhead (single-truth model)
        videoSource.setClockMode(VideoSource::EXTERNAL);
    }
    
    Node::prepare(ctx);
    audioSource.prepare(ctx);
    if (ctx.frames <= 1) {
        videoSource.prepare(ctx);
    }
}

void AVSampler::update(float dt) {
    if (!active->get()) return;

    if (audioSource.audioProcessor) {
        if (audioSource.hasPendingPath()) {
            std::string resolvedPath = audioSource.getPendingPath();
            if (!resolvedPath.empty()) {
                ofLogNotice("AVSampler") << "Trigger path received: " << resolvedPath;
                triggerSampleWithPath(resolvedPath);
            }
        } else if (audioSource.hasPendingTrigger()) {
            int idx = audioSource.getPendingTrigger();
            audioSource.clearPendingTrigger();
            ofLogNotice("AVSampler") << "Trigger index received: " << idx;
            triggerSample(idx);
        }
        if (audioSource.hasPendingRest()) {
            audioSource.clearPendingRest();
            silenceSample();
        }
    }

    if (audioSource.audioProcessor) {
        // SINGLE-TRUTH MODEL: Audio playhead is the authority, video slaves to it
        double currentPos = audioSource.getRelativePosition();
        
        // Always sync video to audio position (single-truth: audio thread drives everything)
        videoSource.setPosition((float)currentPos);
        position.set((float)currentPos);
        
        static int diagCounter = 0;
        if (++diagCounter % 300 == 0) {
            auto* ap = audioSource.audioProcessor;
            if (ap) {
                auto* gainSlot = ap->getControlPtr(crumble::hashString("gain"));
                auto* activeSlot = ap->getControlPtr(crumble::hashString("active"));
                auto* playingSlot = ap->getControlPtr(crumble::hashString("playing"));
                bool hasInputs = false;
                for (int i = 0; i < crumble::AudioProcessor::MAX_INPUTS; i++) {
                    if (ap->inputs[i].processor) { hasInputs = true; break; }
                }
                ofLogNotice("AVSampler") << "diag: pos=" << currentPos
                    << " data=" << (void*)ap->data
                    << " totalSamples=" << ap->totalSamples
                    << " ch=" << ap->channels
                    << " playhead=" << ap->playhead.load()
                    << " gain=" << (gainSlot ? gainSlot->value.load() : -1)
                    << " active=" << (activeSlot ? activeSlot->value.load() : -1)
                    << " playing=" << (playingSlot ? playingSlot->value.load() : -1)
                    << " muted=" << audioSource.getMuted()
                    << " hasUpstreamInputs=" << hasInputs;
            }
        }
    } else {
        if (playing.get() && !videoSource.isPlaying()) {
            videoSource.play();
        }
        position.set(videoSource.getPosition());
    }

    videoSource.update(dt);
    masterPlayhead = audioSource.getRelativePosition();
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
    
    // Reload if path changed
    if (path.get() != newPath) {
        path.set(newPath);
        onParameterChanged("path");
    }
    
    // ALWAYS re-seek to trigger position for true re-trigger effect
    // This ensures that repeated triggers (e.g., "0 0 ~ 0") restart playback
    float trigPos = triggerPosition.get();
    ofLogNotice("AVSampler") << "RETRIGGER: seeking to pos=" << trigPos << " (unmuting)";
    videoSource.setPosition(trigPos);
    audioSource.setRelativePosition(trigPos);
    audioSource.setMuted(false);
    
    // Ensure playing
    if (!playing.get()) {
        playing.set(true);
        onParameterChanged("playing");
    }
}

void AVSampler::triggerSampleWithPath(const std::string& resolvedPath) {
    ofLogNotice("AVSampler") << "triggerSampleWithPath(" << resolvedPath << ")";
    
    // Only reload if path changed
    if (path.get() != resolvedPath) {
        path.set(resolvedPath);
        onParameterChanged("path");
    }
    
    // Seek to trigger position - single-truth: set both, video will slave to audio
    float trigPos = triggerPosition.get();
    ofLogNotice("AVSampler") << "TRIGGER: seeking both to pos=" << trigPos;
    videoSource.setPosition(trigPos);
    audioSource.setRelativePosition(trigPos);
    audioSource.setMuted(false);
    
    // Ensure playing
    if (!playing.get()) {
        playing.set(true);
        onParameterChanged("playing");
    }
}

void AVSampler::silenceSample() {
    // Mute audio - video continues to play to maintain sync
    ofLogNotice("AVSampler") << "REST: muting audio, video continues";
    audioSource.setMuted(true);
}

void AVSampler::modulate(const std::string& paramName, std::shared_ptr<Pattern<float>> pat) {
    // Extract bank name from trigger patterns for sample sequencing
    if (paramName == "n" || paramName == "index") {
        if (auto* seqPat = dynamic_cast<patterns::Seq*>(pat.get())) {
            std::string sig = seqPat->getSignature();
            size_t secondColon = sig.find(':', sig.find(':') + 1);
            if (secondColon != std::string::npos) {
                bankName = sig.substr(secondColon + 1);
            }
        }
        // Send trigger pattern directly to audio processor ("n" is not a regular parameter)
        if (audioProcessor) {
            crumble::ProcessorCommand cmd;
            cmd.type = crumble::ProcessorCommand::SET_PATTERN;
            cmd.paramHash = crumble::hashString("n");
            cmd.pattern = pat;
            pushCommand(cmd);
        }
        // Store for queries
        Node::modulate(paramName, pat);
        return;
    }
    // Pass through to unified pattern system
    Node::modulate(paramName, pat);
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
        if (pathVal.empty()) return;
        std::string vid = resolvePath(pathVal, "video");
        std::string aud = resolvePath(pathVal, "audio");
        
        bool isEmbedded = (aud == vid && !vid.empty());
        
        ofLogNotice("AVSampler") << "path=" << pathVal << " vid=" << vid << " aud=" << aud
                                 << " isEmbedded=" << isEmbedded
                                 << " loadedVideoPath=" << loadedVideoPath;
        
        if (isEmbedded && vid != loadedVideoPath) {
            videoSource.load(vid);
            loadedVideoPath = vid;
            loadedAudioPath = "";
            masterPlayhead = 0.0;
            ofLogNotice("AVSampler") << "Calling loadEmbedded for: " << vid;
            audioSource.loadEmbedded(vid);
        } else if (!isEmbedded) {
            if (!aud.empty() && aud != loadedAudioPath) {
                ofLogNotice("AVSampler") << "Loading audio: " << aud;
                audioSource.load(aud);
                loadedAudioPath = aud;
                masterPlayhead = 0.0;
            }
            if (!vid.empty() && vid != loadedVideoPath) {
                ofLogNotice("AVSampler") << "Loading video: " << vid;
                videoSource.load(vid);
                loadedVideoPath = vid;
                masterPlayhead = 0.0;
            }
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
        float g = gain->get();
        ofLogNotice("AVSampler") << "onParameterChanged(gain): g=" << g << " (nodeId=" << nodeId << ")";
        audioSource.gain->set(g);
        auto pat = getPattern("gain");
        ofLogNotice("AVSampler") << "onParameterChanged(gain): pat=" << (pat ? "set" : "null");
        audioSource.modulate("gain", pat);
        audioSource.onParameterChanged("gain");
    } 
    else if (paramName == "playing") {
        audioSource.playing.set(playing.get());
        videoSource.playing.set(playing.get());
        audioSource.onParameterChanged("playing");
        videoSource.onParameterChanged("playing");
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
