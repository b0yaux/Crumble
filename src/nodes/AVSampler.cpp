#include "AVSampler.h"
#include "../core/Session.h"

AVSampler::AVSampler() {
    type = "AVSampler";
    
    // Add parameters
    parameters.add(audioPath.set("audioPath", ""));
    parameters.add(videoPath.set("videoPath", ""));
    parameters.add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters.add(volume.set("volume", 1.0, 0.0, 1.0));
    parameters.add(loop.set("loop", true));
    parameters.add(playing.set("playing", true));
    parameters.add(position.set("position", 0.0, 0.0, 1.0));
}

void AVSampler::prepare(const Context& ctx) {
    Node::prepare(ctx);
    audioSource.prepare(ctx);
    videoSource.prepare(ctx);
}

void AVSampler::update(float dt) {
    // Update internal sources
    audioSource.update(dt);
    
    // Hard-Sync Video to Audio playhead
    // We do this even when not playing to allow scrubbing while paused
    if (!audioPath.get().empty() && !videoPath.get().empty()) {
        double audioPos = audioSource.getRelativePosition();
        videoSource.setPosition((float)audioPos);
        
        // Update the position parameter for script feedback
        // We use set() which avoids triggering listeners if value hasn't changed much
        position.set((float)audioPos);
    }
    
    videoSource.update(dt);
    
    // Update master playhead for external reference
    masterPlayhead = audioSource.getRelativePosition();
}

void AVSampler::pullAudio(ofSoundBuffer& buffer, int index) {
    if (index != 0) return;
    
    // Pull audio from internal source
    audioSource.pullAudio(buffer, index);
}

ofTexture* AVSampler::getVideoOutput(int index) {
    if (index != 0) return nullptr;
    return videoSource.getVideoOutput(index);
}

std::string AVSampler::getDisplayName() const {
    std::string display = "AVSampler";
    if (!audioPath.get().empty() || !videoPath.get().empty()) {
        if (!videoPath.get().empty()) {
            display += " [" + ofFilePath::getFileName(videoPath.get());
            if (!audioPath.get().empty()) {
                display += " + " + ofFilePath::getFileName(audioPath.get());
            }
            display += "]";
        } else if (!audioPath.get().empty()) {
            display += " [" + ofFilePath::getFileName(audioPath.get()) + "]";
        }
    }
    return display;
}

void AVSampler::onParameterChanged(const std::string& paramName) {
    if (paramName == "audioPath") {
        if (audioPath.get() != loadedAudioPath) {
            audioSource.parameters[std::string("path")].cast<std::string>() = audioPath.get();
            loadedAudioPath = audioPath.get();
            // Reset playhead on new audio
            masterPlayhead = 0.0;
        }
    } else if (paramName == "videoPath") {
        if (videoPath.get() != loadedVideoPath) {
            videoSource.parameters[std::string("path")].cast<std::string>() = videoPath.get();
            loadedVideoPath = videoPath.get();
            // Reset playhead on new video
            masterPlayhead = 0.0;
        }
    } else if (paramName == "speed") {
        // Propagate speed to both sources
        audioSource.parameters[std::string("speed")].cast<float>() = speed.get();
        videoSource.parameters[std::string("speed")].cast<float>() = speed.get();
        
        audioSource.setPattern("speed", getPattern("speed"));
        videoSource.setPattern("speed", getPattern("speed"));
        
        videoSource.onParameterChanged("speed");
    } else if (paramName == "volume") {
        // Propagate volume to audio source
        audioSource.parameters[std::string("volume")].cast<float>() = volume.get();
        audioSource.setPattern("volume", getPattern("volume"));
    } else if (paramName == "loop") {
        // Propagate loop state to both sources
        audioSource.parameters[std::string("loop")].cast<bool>() = loop.get();
        videoSource.parameters[std::string("loop")].cast<bool>() = loop.get();
        videoSource.onParameterChanged("loop");
    } else if (paramName == "playing") {
        // Propagate playing state
        audioSource.parameters[std::string("playing")].cast<bool>() = playing.get();
        videoSource.parameters[std::string("playing")].cast<bool>() = playing.get();
        videoSource.onParameterChanged("playing");
    } else if (paramName == "position") {
        // Seek both sources to the new position
        audioSource.setRelativePosition(position.get());
        videoSource.setPosition(position.get());
        // Force immediate update of master playhead
        masterPlayhead = position.get();
    }
}

void AVSampler::setMasterPlayhead(double position) {
    masterPlayhead = position;
    
    // For now, this is a placeholder for future position/seek functionality
    // When we implement position parameter, this will sync both sources to the same playhead
}

ofJson AVSampler::serialize() const {
    ofJson j;
    ofSerialize(j, parameters);
    return j;
}

void AVSampler::deserialize(const ofJson& json) {
    ofJson j = json;
    
    // Handle common nesting patterns
    if (j.contains("AVSampler")) {
        j = j["AVSampler"];
    } else if (j.contains("group")) {
        j = j["group"];
    }
    
    // Load parameters
    if (j.contains("audioPath")) {
        audioPath.set(getSafeJson<std::string>(j, "audioPath", ""));
    }
    if (j.contains("videoPath")) {
        videoPath.set(getSafeJson<std::string>(j, "videoPath", ""));
    }
    if (j.contains("speed")) {
        speed.set(getSafeJson<float>(j, "speed", 1.0f));
    }
    if (j.contains("volume")) {
        volume.set(getSafeJson<float>(j, "volume", 1.0f));
    }
    if (j.contains("loop")) {
        loop.set(getSafeJson<bool>(j, "loop", true));
    }
    if (j.contains("playing")) {
        playing.set(getSafeJson<bool>(j, "playing", true));
    }
    
    ofDeserialize(j, parameters);
}
