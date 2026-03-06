#include "VideoFileSource.h"

VideoFileSource::VideoFileSource() {
    type = "VideoFileSource";

    parameters.add(path.set("path", ""));
    parameters.add(loop.set("loop", true));
    parameters.add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters.add(playing.set("playing", true));
    parameters.add(clockMode.set("clockMode", VideoFileSource::INTERNAL, VideoFileSource::INTERNAL, VideoFileSource::EXTERNAL));

    path.addListener(this, &VideoFileSource::onPathChanged);
    clockMode.addListener(this, &VideoFileSource::onClockModeChanged);
}

void VideoFileSource::onClockModeChanged(int& mode) {
    if (player.isLoaded()) {
        if (mode == VideoFileSource::EXTERNAL) {
            player.setSpeed(0.0f);
            player.setPaused(true);
        } else {
            if (playing) {
                player.play();
                player.setSpeed(speed);
            }
        }
    }
}

void VideoFileSource::onPathChanged(std::string& p) {
    if (!p.empty() && p != loadedPath) {
        load(p);
    }
}


void VideoFileSource::load(const std::string& vidPath) {
    // 1. Resolve via base class utility (proxies to Graph -> Registry)
    std::string resolvedPath = resolvePath(vidPath, "video");
    
    ofLogNotice("VideoFileSource") << "Loading video: " << resolvedPath << " (from: " << vidPath << ")";
    
    bool loaded = player.load(resolvedPath);

    if (loaded) {
        player.setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
        
        if (clockMode == VideoFileSource::INTERNAL) {
            player.setSpeed(speed);
            if (playing) {
                player.play();
            }
        } else {
            // EXTERNAL mode relies entirely on manual scrubbing
            player.setSpeed(0.0f); // Default to stopped speed, or maybe not needed
            player.setPaused(true);
        }
        loadedPath = vidPath;
    } else {
        ofLogError("VideoFileSource") << "Failed to load: " << resolvedPath;
    }
}

void VideoFileSource::update(float dt) {
    if (graph) {
        Control c = getControl(speed);
        float s = c[0];
        
        if (clockMode == VideoFileSource::INTERNAL) {
            player.setSpeed(s);
        } else {
            // In EXTERNAL mode, we don't set speed.
            // The playhead is exclusively driven by setPosition() or setFrame()
        }
    }
    player.update();
    
    // Auto-play safety: ensure playing if it's supposed to be
    if (clockMode == VideoFileSource::INTERNAL && playing && player.isLoaded() && !player.isPlaying() && !player.isPaused()) {
        player.play();
    } else if (clockMode == VideoFileSource::EXTERNAL && player.isLoaded() && !player.isPaused()) {
        // Enforce pause state for external mode
        player.setPaused(true);
    }
}

void VideoFileSource::onParameterChanged(const std::string& paramName) {
    if (!player.isLoaded()) return;

    if (paramName == "speed") {
        if (clockMode == VideoFileSource::INTERNAL) {
            player.setSpeed(speed);
        }
    } else if (paramName == "loop") {
        player.setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
    } else if (paramName == "playing") {
        if (clockMode == VideoFileSource::INTERNAL) {
            if (playing) player.play();
            else player.setPaused(true);
        }
    }
}

ofTexture* VideoFileSource::processVideo(int index) {
    if (index != 0) return nullptr;
    if (player.isLoaded()) {
        ofTexture* tex = player.getTexture();
        if (tex && tex->isAllocated()) {
            return tex;
        }
    }
    
    // Return nullptr when no video loaded - layer will be transparent
    return nullptr;
}

std::string VideoFileSource::getDisplayName() const {
    std::string p = path.get();
    if (p.empty()) return name;
    size_t lastSlash = p.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return p.substr(lastSlash + 1);
    }
    return p;
}

void VideoFileSource::play() {
    player.play();
}

void VideoFileSource::stop() {
    player.stop();
}

void VideoFileSource::pause() {
    player.setPaused(true);
}

bool VideoFileSource::isPlaying() const {
    return player.isPlaying();
}

void VideoFileSource::setFrame(int frame) {
    player.setFrame(frame);
}

void VideoFileSource::setPosition(float pct) {
    player.setPosition(pct);
}

int VideoFileSource::getCurrentFrame() const {
    return player.getCurrentFrame();
}

int VideoFileSource::getTotalFrames() const {
    return player.getTotalNumFrames();
}

void VideoFileSource::setLoop(bool shouldLoop) {
    loop = shouldLoop;
    player.setLoopState(shouldLoop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
}

ofJson VideoFileSource::serialize() const {
    ofJson j;
    ofSerialize(j, parameters);
    return j;
}

void VideoFileSource::deserialize(const ofJson& json) {
    ofJson j = json;
    if (j.contains("group")) {
        j = j["group"];
    } else if (j.contains("params")) {
        j = j["params"];
    }

    // Manually extract parameters with "loose" type support to prevent Abort trap
    // Migrate from old "videoPath" key to new "path" key
    std::string pathValue;
    if (j.contains("videoPath")) {
        pathValue = getSafeJson<string>(j, "videoPath", "");
    } else if (j.contains("path")) {
        pathValue = getSafeJson<string>(j, "path", "");
    }
    if (!pathValue.empty()) path.set(pathValue);

    if (j.contains("loop")) loop = getSafeJson<bool>(j, "loop", loop.get());
    if (j.contains("speed")) speed = getSafeJson<float>(j, "speed", speed.get());
    if (j.contains("playOnLoad")) playing = getSafeJson<bool>(j, "playOnLoad", playing.get());
    if (j.contains("playing")) playing = getSafeJson<bool>(j, "playing", playing.get());

    ofDeserialize(j, parameters);
}
