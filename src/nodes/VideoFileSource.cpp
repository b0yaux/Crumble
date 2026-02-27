#include "VideoFileSource.h"

VideoFileSource::VideoFileSource() {
    type = "VideoFileSource";

    parameters.add(path.set("path", ""));
    parameters.add(loop.set("loop", true));
    parameters.add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters.add(playOnLoad.set("playOnLoad", true));

    path.addListener(this, &VideoFileSource::onPathChanged);
}

void VideoFileSource::onPathChanged(std::string& path) {
    if (!path.empty() && path != loadedPath) {
        load(path);
    }
}


void VideoFileSource::load(const std::string& vidPath) {
    path = vidPath;
    
    // Check if the path is absolute or relative to data path
    string fullPath = ofToDataPath(vidPath);
    if (!ofFile::doesFileExist(fullPath)) {
        // Try as-is in case it's an absolute path outside data
        fullPath = vidPath;
    }
    
    ofLogNotice("VideoFileSource") << "Attempting to load: " << fullPath;
    
    bool loaded = player.load(fullPath);
    if (!loaded && ofFilePath::isAbsolute(fullPath)) {
        // One last try: check if it's a relative path that was mistaken for absolute
        string relPath = ofToDataPath(vidPath, false);
        ofLogNotice("VideoFileSource") << "Retrying as relative: " << relPath;
        loaded = player.load(relPath);
    }

    if (loaded) {
        player.setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
        player.setSpeed(speed);
        
        if (playOnLoad) {
            player.play();
        }
        loadedPath = vidPath;
        ofLogNotice("VideoFileSource") << "Successfully loaded and playing: " << fullPath;
    } else {
        ofLogError("VideoFileSource") << "Failed to load: " << fullPath;
    }
}

void VideoFileSource::update(float dt) {
    player.update();
    
    // Auto-play safety: ensure playing if it's supposed to be
    if (playOnLoad && player.isLoaded() && !player.isPlaying() && !player.isPaused()) {
        player.play();
    }
}

void VideoFileSource::onParameterChanged(const std::string& paramName) {
    if (paramName == "speed" && player.isLoaded()) {
        player.setSpeed(speed);
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
    if (j.contains("playOnLoad")) playOnLoad = getSafeJson<bool>(j, "playOnLoad", playOnLoad.get());

    ofDeserialize(j, parameters);
}
