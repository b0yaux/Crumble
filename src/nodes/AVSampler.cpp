#include "AVSampler.h"

AVSampler::AVSampler() {
    type = "AVSampler";
    
    // Add parameters
    parameters.add(path.set("path", ""));
    parameters.add(audioPath.set("audioPath", ""));
    parameters.add(videoPath.set("videoPath", ""));
    parameters.add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters.add(loop.set("loop", true));
    parameters.add(playing.set("playing", true));
    parameters.add(position.set("position", 0.0, 0.0, 1.0));
    
    // Performance: Cache direct pointers to child parameters to avoid map lookups
    cachedAudioSpeed = &audioSource.speed;
    cachedVideoSpeed = &videoSource.speed;
    cachedAudioVolume = &audioSource.volume;
    cachedAudioLoop = &audioSource.loop;
    cachedVideoLoop = &videoSource.loop;
    cachedAudioPlaying = &audioSource.playing;
    cachedVideoPlaying = &videoSource.playing;

    lastSyncPos = -1.0;
}

void AVSampler::prepare(const Context& ctx) {
    // Optimization: Only update internal source configuration if graph changes.
    if (graph && (audioSource.graph != graph || videoSource.graph != graph)) {
        audioSource.graph = graph;
        videoSource.graph = graph;
        
        // Ensure video is in external clock mode for perfect A/V sync
        if (videoSource.parameters.contains("clockMode")) {
            videoSource.parameters.get("clockMode").cast<int>().set(VideoFileSource::EXTERNAL);
        }
    }
    
    Node::prepare(ctx);
    
    // Audio source must be prepared to calculate its internal modulators
    audioSource.prepare(ctx);

    // Video source only needs preparation during the UI update pass (ctx.frames == 1)
    // In audio blocks, its internal modulators are ignored due to EXTERNAL clock mode.
    if (ctx.frames <= 1) {
        videoSource.prepare(ctx);
    }
}

void AVSampler::update(float dt) {
    audioSource.update(dt);
    
    // Simple Sync: Force video to follow audio playhead.
    if (!audioPath.get().empty() && !videoPath.get().empty()) {
        double audioPos = audioSource.getRelativePosition();
        
        // Only seek if there's a significant jump to avoid overwhelming the decoder
        if (std::abs(audioPos - lastSyncPos) > 0.001) {
            videoSource.setPosition((float)audioPos);
            lastSyncPos = audioPos;
        }

        // Guard against recursive 'onParameterChanged' storm
        isInternalChange = true;
        position.set((float)audioPos);
        isInternalChange = false;
    }
    
    videoSource.update(dt);
    masterPlayhead = audioSource.getRelativePosition();
}

void AVSampler::processAudio(ofSoundBuffer& buffer, int index) {
    if (index == 0) audioSource.pullAudio(buffer, index);
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
    // 1. Handle Unified Path (Macro)
    if (paramName == "path") {
        std::string pathVal = path.get();
        if (pathVal.empty()) return;

        std::string vid = resolvePath(pathVal, "video");
        std::string aud = resolvePath(pathVal, "audio");
        
        if (aud != audioPath.get()) {
            audioPath.set(aud);
            onParameterChanged("audioPath"); // Crucial: trigger load
        }
        if (vid != videoPath.get()) {
            videoPath.set(vid);
            onParameterChanged("videoPath"); // Crucial: trigger load
        }
    } 
    // 2. Handle Individual Streams (The actual loading logic)
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
    // 3. Propagate Modulators and State
    else if (paramName == "speed") {
        if (cachedAudioSpeed) cachedAudioSpeed->set(speed.get());
        if (cachedVideoSpeed) cachedVideoSpeed->set(speed.get());
        audioSource.modulate("speed", getPattern("speed"));
        videoSource.modulate("speed", getPattern("speed"));
    } else if (paramName == "volume") {
        if (cachedAudioVolume) cachedAudioVolume->set(volume.get());
        audioSource.modulate("volume", getPattern("volume"));
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
        audioSource.active.set(active.get());
        videoSource.active.set(active.get());
    }
}

void AVSampler::setMasterPlayhead(double pos) { masterPlayhead = pos; }

ofJson AVSampler::serialize() const {
    ofJson j;
    ofSerialize(j, parameters);
    return j;
}

void AVSampler::deserialize(const ofJson& json) {
    ofJson j = json;
    if (j.contains("AVSampler")) j = j["AVSampler"];
    else if (j.contains("group")) j = j["group"];
    
    if (j.contains("path")) path.set(getSafeJson<std::string>(j, "path", ""));
    if (j.contains("audioPath")) audioPath.set(getSafeJson<std::string>(j, "audioPath", ""));
    if (j.contains("videoPath")) videoPath.set(getSafeJson<std::string>(j, "videoPath", ""));
    
    ofDeserialize(j, parameters);
}
