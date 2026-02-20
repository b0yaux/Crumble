#include "VideoFileSource.h"

VideoFileSource::VideoFileSource() {
    type = "VideoFileSource";
    
    parameters.add(videoPath.set("videoPath", ""));
    parameters.add(loop.set("loop", true));
    parameters.add(speed.set("speed", 1.0, -2.0, 2.0));
    parameters.add(playOnLoad.set("playOnLoad", true));
}

void VideoFileSource::load(const std::string& vidPath) {
    videoPath = vidPath;
    
    if (player.load(vidPath)) {
        player.setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
        player.setSpeed(speed);
        
        if (playOnLoad) {
            player.play();
        }
    } else {
        ofLogError("VideoFileSource") << "Failed to load: " << vidPath;
    }
}

void VideoFileSource::update(float dt) {
    player.update();
}

ofTexture* VideoFileSource::getVideoOutput() {
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
    std::string path = videoPath.get();
    if (path.empty()) return name;
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return path.substr(lastSlash + 1);
    }
    return path;
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

void VideoFileSource::deserializeComplete() {
    std::string path = videoPath.get();
    if (!path.empty() && !player.isLoaded()) {
        load(path);
    }
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
    }
    
    // Manually extract videoPath since ofDeserialize may have issues with the format
    if (j.contains("videoPath")) {
        videoPath = j["videoPath"].get<std::string>();
    }
    
    ofDeserialize(j, parameters);
}
