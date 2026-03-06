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
    // 1. Skip if node is inactive
    if (!active) return;

    // 2. Optimization: Sync graph and clockMode only when the graph changes.
    if (graph && (audioSource.graph != graph || videoSource.graph != graph)) {
        audioSource.graph = graph;
        videoSource.graph = graph;
        if (videoSource.parameters.contains("clockMode")) {
            videoSource.parameters.get("clockMode").cast<int>().set(VideoFileSource::EXTERNAL);
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
    // FIX: If the node is inactive, skip the update entirely to prevent seek storms.
    if (!active) return;

    // NOTE: audioSource.update is strictly handled by the audio thread.
    
    if (!audioPath.get().empty() && !videoPath.get().empty()) {
        double audioPos = audioSource.getRelativePosition();
        
        // Performance: Only seek if the playhead moved significantly.
        if (std::abs(audioPos - lastSyncPos) > 0.001) {
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
