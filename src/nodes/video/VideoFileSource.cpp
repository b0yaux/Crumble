#include "VideoFileSource.h"
#include "ofxAudioFile.h"

VideoFileSource::VideoFileSource() {
    type = "VideoFileSource";
    
    parameters.add(videoPath.set("videoPath", ""));
    parameters.add(audioPath.set("audioPath", ""));
    parameters.add(loop.set("loop", true));
    parameters.add(speed.set("speed", 1.0, -2.0, 2.0));
    parameters.add(playOnLoad.set("playOnLoad", true));
}

void VideoFileSource::load(const std::string& vidPath, const std::string& audPath) {
    videoPath = vidPath;
    audioFilePath = audPath;
    hasAudioFile = !audPath.empty();
    
    if (player.load(vidPath)) {
        player.setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
        player.setSpeed(speed);
        
        if (playOnLoad) {
            player.play();
        }
        
        // Load external audio if specified
        if (hasAudioFile) {
            ofxAudioFile audioFile;
            audioFile.load(audPath);
            // External audio loaded - assume success
            useEmbeddedAudio = false;
        }
        
        ofLogNotice("VideoFileSource") << "Loaded: " << vidPath;
    } else {
        ofLogError("VideoFileSource") << "Failed to load: " << vidPath;
    }
}

void VideoFileSource::loadPaired(const std::string& basePath) {
    std::string vidPath = basePath + ".mov";
    std::string audPath = basePath + ".wav";
    load(vidPath, audPath);
}

void VideoFileSource::update(float dt) {
    player.update();
    
    // Sync audio if using external file
    if (hasAudioFile && !useEmbeddedAudio && player.isPlaying()) {
        // Audio sync logic would go here
        // For now, we'll use embedded audio from HAP player
    }
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

ofSoundBuffer* VideoFileSource::getAudioOutput() {
    // HAP player handles audio internally
    // For external audio file, we'd need to manage the buffer ourselves
    return nullptr;
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
