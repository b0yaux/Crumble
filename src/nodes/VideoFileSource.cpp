#include "VideoFileSource.h"

VideoFileSource::VideoFileSource() {
    type = "VideoFileSource";

    parameters.add(path.set("path", ""));
    parameters.add(loop.set("loop", true));
    parameters.add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters.add(playing.set("playing", true));

    path.addListener(this, &VideoFileSource::onPathChanged);
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
        player.setSpeed(speed);
        
        if (playing) {
            player.play();
        }
        loadedPath = vidPath;
    } else {
        ofLogError("VideoFileSource") << "Failed to load: " << resolvedPath;
    }
}

void VideoFileSource::update(float dt) {
    if (graph) {
        Control c = getControl(speed);
        player.setSpeed(c[0]);
    }
    player.update();
    
    // Auto-play safety: ensure playing if it's supposed to be
    if (playing && player.isLoaded() && !player.isPlaying() && !player.isPaused()) {
        player.play();
    }
}

void VideoFileSource::onParameterChanged(const std::string& paramName) {
    if (!player.isLoaded()) return;

    if (paramName == "speed") {
        player.setSpeed(speed);
    } else if (paramName == "loop") {
        player.setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
    } else if (paramName == "playing") {
        if (playing) player.play();
        else player.setPaused(true);
    }
}

ofTexture* VideoFileSource::getVideoOutput(int index) {
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
