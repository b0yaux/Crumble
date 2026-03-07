#include "AVSampler.h"
#include "ofMain.h"

AVSampler::AVSampler() {
    type = "AVSampler";
    
    // Add parameters
    parameters->add(path.set("path", ""));
    parameters->add(audioPath.set("audioPath", ""));
    parameters->add(videoPath.set("videoPath", ""));
    parameters->add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters->add(loop.set("loop", true));
    parameters->add(playing.set("playing", true));
    parameters->add(position.set("position", 0.0, 0.0, 1.0));
    
    // NOTE: Processors are created via createAudioProcessor() and createVideoProcessor()
    // when Graph::createNode() calls setupProcessor() -> createAudioProcessor()/createVideoProcessor()
    
    // Performance: Cache direct pointers to child parameters to avoid map lookups
    cachedAudioSpeed = &audioSource.speed;
    cachedVideoSpeed = &videoSource.speed;
    cachedAudioVolume = audioSource.volume.get();
    cachedAudioLoop = &audioSource.loop;
    cachedVideoLoop = &videoSource.loop;
    cachedAudioPlaying = &audioSource.playing;
    cachedVideoPlaying = &videoSource.playing;

    lastSyncPos = -1.0;
}

AVSampler::~AVSampler() {
    audioSource.audioProcessor = nullptr;
}

crumble::AudioProcessor* AVSampler::createAudioProcessor() {
    // Create the AudioFileProcessor via the audioSource factory
    crumble::AudioProcessor* proc = audioSource.createAudioProcessor();
    if (proc) {
        audioSource.audioProcessor = proc;
        proc->nodeId = audioSource.nodeId;

        // Populate valuesMap from audioSource's parameter group
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
                proc->valuesMap[p.getName()].store(val);
            }
        }
    }
    return proc;
}

crumble::VideoProcessor* AVSampler::createVideoProcessor() {
    // Create the VideoFileProcessor via the videoSource factory
    crumble::VideoProcessor* proc = videoSource.createVideoProcessor();
    if (proc) {
        proc->nodeId = videoSource.nodeId;

        // Populate valuesMap from videoSource's parameter group
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
                proc->valuesMap[p.getName()].store(val);
            }
        }
        
        // Also copy AVSampler's own parameters (opacity, volume, active)
        // These are what the VideoMixer needs to read from the source
        for (int i = 0; i < (int)parameters->size(); i++) {
            auto& p = parameters->get(i);
            std::string name = p.getName();
            if (name == "opacity" || name == "volume" || name == "active") {
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
                    proc->valuesMap[name].store(val);
                }
            }
        }
    }
    return proc;
}

void AVSampler::prepare(const Context& ctx) {
    // 1. Skip if node is inactive
    if (!active->get()) return;

    // 2. Optimization: Sync graph and clockMode only when the graph changes.
    if (graph && (audioSource.graph != graph || videoSource.graph != graph)) {
        audioSource.graph = graph;
        videoSource.graph = graph;
        if (videoSource.parameters->contains("clockMode")) {
            videoSource.parameters->get("clockMode").cast<int>().set(VideoFileSource::INTERNAL);
        }
    }
    
    // 3. Base prepare
    Node::prepare(ctx);
    
    // 4. Selective Prepare: only prepare audio on audio thread
    audioSource.prepare(ctx);
    if (ctx.frames <= 1) {
        videoSource.prepare(ctx);
    }
}

void AVSampler::update(float dt) {
    if (!active->get()) return;

    if (!audioPath.get().empty() && !videoPath.get().empty()) {
        double audioPos = audioSource.getRelativePosition();
        
        // PASSIVE SYNC (Soft Sync)
        // We rely on the fact that AudioFileSource and VideoFileSource share the same 
        // internal 'speed' multiplier, so they run parallel to each other naturally.
        // Calling setPosition() on a HAP player forces its internal demuxer to flush 
        // its background decoding queue, which causes severe I/O and CPU stalls if 
        // done every frame across many layers.
        // We only force a "Hard Sync" jump if the drift exceeds our tolerance threshold.
        const double DRIFT_TOLERANCE = 0.05; // 5% divergence
        if (std::abs(audioPos - lastSyncPos) > DRIFT_TOLERANCE) {
            videoSource.setPosition((float)audioPos);
            lastSyncPos = audioPos;
        }

        // Internal change guard used to prevent recursion
        isInternalChange = true;
        position.set((float)audioPos);
        isInternalChange = false;
    }
    
    videoSource.update(dt);
    masterPlayhead = audioSource.getRelativePosition();
}

void AVSampler::setupProcessor() {
    // Crucial: Children must share the SAME ID as parent for shadow worker routing
    audioSource.nodeId = nodeId;
    videoSource.nodeId = nodeId;
    
    Node::setupProcessor();
}

void AVSampler::processAudio(ofSoundBuffer& buffer, int index) {
    // DSP is handled by the internal audioSource's processor
}

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
        if (aud != audioPath.get()) {
            audioPath.set(aud);
            onParameterChanged("audioPath");
        }
        if (vid != videoPath.get()) {
            videoPath.set(vid);
            onParameterChanged("videoPath");
        }
    } 
    else if (paramName == "audioPath") {
        if (!audioPath.get().empty() && audioPath.get() != loadedAudioPath) {
            audioSource.load(audioPath.get());
            loadedAudioPath = audioPath.get();
            masterPlayhead = 0.0;
        }
    } else if (paramName == "videoPath") {
        if (!videoPath.get().empty() && videoPath.get() != loadedVideoPath) {
            videoSource.load(videoPath.get());
            loadedVideoPath = videoPath.get();
            masterPlayhead = 0.0;
        }
    } 
    else if (paramName == "speed") {
        ofLogNotice("AVSampler") << "onParameterChanged: speed - propagating to children";
        if (cachedAudioSpeed) cachedAudioSpeed->set(speed.get());
        if (cachedVideoSpeed) cachedVideoSpeed->set(speed.get());
        
        auto pat = getPattern("speed");
        ofLogNotice("AVSampler") << "  pattern for speed: " << (pat ? pat->getSignature() : "null");
        audioSource.modulate("speed", pat);
        audioSource.onParameterChanged("speed");
        videoSource.modulate("speed", pat);
        videoSource.onParameterChanged("speed");
    } else if (paramName == "volume") {
        if (cachedAudioVolume) cachedAudioVolume->set(volume->get());
        auto pat = getPattern("volume");
        audioSource.modulate("volume", pat);
        audioSource.onParameterChanged("volume");
    } 
    else if (paramName == "opacity") {
        auto pat = getPattern("opacity");
        videoSource.modulate("opacity", pat);
        videoSource.onParameterChanged("opacity");
    } else if (paramName == "loop") {
        if (cachedAudioLoop) cachedAudioLoop->set(loop.get());
        if (cachedVideoLoop) cachedVideoLoop->set(loop.get());
    } else if (paramName == "playing") {
        if (cachedAudioPlaying) cachedAudioPlaying->set(playing.get());
        if (cachedVideoPlaying) cachedVideoPlaying->set(playing.get());
    } else if (paramName == "position") {
        if (!isInternalChange) {
            audioSource.setRelativePosition(position.get());
            videoSource.setPosition(position.get());
        }
    } else if (paramName == "active") {
        bool isActive = active->get();
        audioSource.active->set(isActive);
        videoSource.active->set(isActive);
        // Also mirror to 'playing' on the audioSource so the AudioFileProcessor
        // (which only checks getParam("playing") in the audio thread) goes silent.
        // We only force-stop; we don't auto-start — the user's 'playing' param wins on resume.
        if (!isActive) {
            audioSource.playing.set(false);
        } else {
            // Restore the AVSampler's own playing state
            audioSource.playing.set(playing.get());
        }
        audioSource.onParameterChanged("playing");
    }
    
    // Always propagate to processors for unhandled params (like opacity, volume)
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
