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
}

void AVSampler::prepare(const Context& ctx) {
    // Crucial: Propagate graph context to internal sub-nodes for path resolution
    if (graph) {
        audioSource.graph = graph;
        videoSource.graph = graph;
        
        // Ensure video is in external clock mode for perfect sync
        videoSource.parameters.get("clockMode").cast<int>() = VideoFileSource::EXTERNAL;
    }
    
    Node::prepare(ctx);
    audioSource.prepare(ctx);
    videoSource.prepare(ctx);
}

void AVSampler::update(float dt) {
    audioSource.update(dt);
    
    // Simple Sync: Force video to follow audio playhead.
    // HAP is efficient enough for this if we keep the decoder hot.
    if (!audioPath.get().empty() && !videoPath.get().empty()) {
        double audioPos = audioSource.getRelativePosition();
        
        videoSource.setPosition((float)audioPos);

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

        // Resolve logical components via base class utility
        std::string vid = resolvePath(pathVal, "video");
        std::string aud = resolvePath(pathVal, "audio");
        
        // Propagate to internal parameters
        // Setting these will trigger the loading logic below.
        if (aud != audioPath.get()) {
            audioPath.set(aud);
            onParameterChanged("audioPath");
        }
        if (vid != videoPath.get()) {
            videoPath.set(vid);
            onParameterChanged("videoPath");
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
        audioSource.parameters[std::string("speed")].cast<float>() = speed.get();
        videoSource.parameters[std::string("speed")].cast<float>() = speed.get();
        audioSource.modulate("speed", getPattern("speed"));
        videoSource.modulate("speed", getPattern("speed"));
    } else if (paramName == "volume") {
        // Still propagate volume to internal audioSource so its internal logic works
        audioSource.parameters[std::string("volume")].cast<float>() = volume.get();
        audioSource.modulate("volume", getPattern("volume"));
    } else if (paramName == "opacity") {
        // Still propagate opacity to internal videoSource if it has it
        if (videoSource.parameters.contains("opacity")) {
            videoSource.parameters[std::string("opacity")].cast<float>() = opacity.get();
            videoSource.modulate("opacity", getPattern("opacity"));
        }
    } else if (paramName == "loop") {
        audioSource.parameters[std::string("loop")].cast<bool>() = loop.get();
        videoSource.parameters[std::string("loop")].cast<bool>() = loop.get();
    } else if (paramName == "playing") {
        audioSource.parameters[std::string("playing")].cast<bool>() = playing.get();
        videoSource.parameters[std::string("playing")].cast<bool>() = playing.get();
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
    
    // Set parameters (setter logic handles the rest)
    if (j.contains("path")) path.set(getSafeJson<std::string>(j, "path", ""));
    if (j.contains("audioPath")) audioPath.set(getSafeJson<std::string>(j, "audioPath", ""));
    if (j.contains("videoPath")) videoPath.set(getSafeJson<std::string>(j, "videoPath", ""));
    
    ofDeserialize(j, parameters);
}
