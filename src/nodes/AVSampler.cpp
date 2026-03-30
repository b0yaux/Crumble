#include "AVSampler.h"
#include "ofMain.h"
#include "../core/NodeProcessor.h"

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
        // SINGLE-TRUTH SYNC: Audio playhead is the authority. 
        // We calculate the current relative position from the audio thread's playhead.
        double currentPos = audioSource.getRelativePosition();
        
        // Slave the video player to the audio position.
        // In EXTERNAL mode, this ensures perfect A/V sync without drift.
        videoSource.setPosition((float)currentPos);
        position.set((float)currentPos);
    } else {
        if (playing.get() && !videoSource.isPlaying()) {
            videoSource.play();
        }
        position.set(videoSource.getPosition());
    }

    // Keep the video player heartbeat alive for decoding.
    videoSource.update(dt);
    masterPlayhead = audioSource.getRelativePosition();
}

void AVSampler::triggerSample(int index) {
    std::string b = audioSource.bank.get();
    std::string newPath;
    if (!b.empty()) {
        newPath = b + ":" + std::to_string(index);
    } else {
        newPath = path.get().empty() ? std::to_string(index) : path.get();
    }
    
    ofLogNotice("AVSampler") << "triggerSample(" << index << ") -> path: " << newPath;
    
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
        auto pat = getPattern("path");
        if (pat) {
            Node::onParameterChanged(paramName);
            return;
        }
        
        // Static path: resolve and load media files
        std::string pathVal = path.get();
        if (pathVal.empty()) return;
        std::string vid = resolvePath(pathVal, "video");
        std::string aud = resolvePath(pathVal, "audio");

        if (audioSource.bank.get().empty()) audioSource.bank.set(Node::extractBank(pathVal));
        if (videoSource.bank.get().empty()) videoSource.bank.set(Node::extractBank(pathVal));
        
        bool isEmbedded = (aud == vid && !vid.empty());
        
        ofLogNotice("AVSampler") << "path=" << pathVal << " vid=" << vid << " aud=" << aud
                                 << " isEmbedded=" << isEmbedded
                                 << " loadedVideoPath=" << loadedVideoPath;
        
        if (isEmbedded && vid != loadedVideoPath) {
            videoSource.load(vid);
            loadedVideoPath = vid;
            loadedAudioPath = "";
            masterPlayhead = 0.0;
            ofLogNotice("AVSampler") << "Loading embedded audio via load for: " << vid;
            audioSource.load(vid);
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
