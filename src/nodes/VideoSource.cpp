#include "ofMain.h"
#include "VideoSource.h"
#include "../core/NodeProcessor.h"

// Shadow processor for the video thread
class VideoSourceProcessor : public crumble::VideoProcessor {
public:
    crumble::ControlSlot* speedSlot = nullptr;
    crumble::ControlSlot* playingSlot = nullptr;
    crumble::ControlSlot* activeSlot = nullptr;

    VideoSourceProcessor(ofxHapPlayer* p) : playerRef(p), lastSpeed(1.0f) {
        speedSlot = getControlPtr(crumble::hashString("speed"));
        playingSlot = getControlPtr(crumble::hashString("playing"));
        activeSlot = getControlPtr(crumble::hashString("active"));
    }

    void processVideo(double cycle, double cycleStep) override {
        if (!playerRef || !playerRef->isLoaded()) return;

        float newSpeed = evalSlot(speedSlot, cycle);

        if (std::abs(newSpeed - lastSpeed) > 1e-4f) {
            if (newSpeed == 0.0f) {
                playerRef->setPaused(true);
            } else {
                if (lastSpeed == 0.0f && evalSlot(playingSlot, cycle) > 0.5f) {
                    playerRef->setPaused(false);
                }
                playerRef->setSpeed(newSpeed);
            }
            lastSpeed = newSpeed;
        }

        playerRef->update();
    }
    
    ofTexture* getOutput(int index = 0) override {
        if (evalSlot(activeSlot, currentCycle) < 0.5f) return nullptr;
        
        if (playerRef && playerRef->isLoaded()) {
            ofTexture* tex = playerRef->getTexture();
            if (tex && tex->isAllocated()) return tex;
        }
        return nullptr;
    }
    
private:
    ofxHapPlayer* playerRef = nullptr;
    float lastSpeed;
};

VideoSource::VideoSource() {
    type = "video";

    parameters->add(path.set("path", ""));
    parameters->add(loop.set("loop", true));
    parameters->add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters->add(playing.set("playing", true));
    parameters->add(clockMode.set("clockMode", VideoSource::INTERNAL, VideoSource::INTERNAL, VideoSource::EXTERNAL));

    path.addListener(this, &VideoSource::onPathChanged);
    clockMode.addListener(this, &VideoSource::onClockModeChanged);
}

crumble::VideoProcessor* VideoSource::createVideoProcessor() {
    return new VideoSourceProcessor(&player);
}

void VideoSource::setClockMode(ClockMode mode) {
    clockMode.set(static_cast<int>(mode));
}

void VideoSource::safeSetPlayerSpeed(float newSpeed) {
    if (newSpeed == 0.0f) {
        player.setPaused(true);
    } else {
        if (lastSpeed == 0.0f && playing.get()) {
            player.setPaused(false);
        }
        player.setSpeed(newSpeed);
    }
    lastSpeed = newSpeed;
}

void VideoSource::onClockModeChanged(int& mode) {
    if (player.isLoaded()) {
        if (mode == VideoSource::EXTERNAL) {
            player.setPaused(true);
        } else {
            if (playing) {
                player.play();
                safeSetPlayerSpeed(speed.get());
            }
        }
    }
}

void VideoSource::onPathChanged(std::string& p) {
    if (!p.empty() && p != loadedPath) {
        load(p);
    }
}


void VideoSource::load(const std::string& vidPath) {
    std::string resolvedPath = resolvePath(vidPath, "video");
    ofLogNotice("VideoSource") << "Loading video: " << resolvedPath << " (from: " << vidPath << ")";
    
    bool loaded = player.load(resolvedPath);

    if (loaded) {
        player.setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
        
        // Check for embedded audio stream via ofxHapPlayer
        _hasAudio = (player.getAudioOutput() != nullptr);
        
        // Don't mute - VideoEmbedAudioProcessor will handle muting for RESTs
        // player.setVolume would silence the audioOut() buffer completely
        
        if (clockMode == VideoSource::INTERNAL) {
            if (playing) player.play();
            safeSetPlayerSpeed(speed.get());
        } else {
            player.setPaused(true);
        }
        loadedPath = vidPath;
    } else {
        ofLogError("VideoSource") << "Failed to load: " << resolvedPath;
        _hasAudio = false;
    }
}

void VideoSource::onParameterChanged(const std::string& paramName) {
    if (!player.isLoaded()) return;

    if (paramName == "speed") {
        if (clockMode == VideoSource::INTERNAL) {
            safeSetPlayerSpeed(speed.get());
        }
    } else if (paramName == "loop") {
        player.setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
    } else if (paramName == "playing") {
        if (clockMode == VideoSource::INTERNAL) {
            if (playing) player.play();
            else player.setPaused(true);
        }
    }
}

ofTexture* VideoSource::processVideo(int index) {
    if (index != 0) return nullptr;
    if (player.isLoaded()) {
        ofTexture* tex = player.getTexture();
        if (tex && tex->isAllocated()) {
            return tex;
        }
    }
    return nullptr;
}

std::string VideoSource::getDisplayName() const {
    std::string p = path.get();
    if (p.empty()) return name;
    size_t lastSlash = p.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return p.substr(lastSlash + 1);
    }
    return p;
}

void VideoSource::play() {
    player.play();
}

void VideoSource::stop() {
    player.stop();
}

void VideoSource::pause() {
    player.setPaused(true);
}

bool VideoSource::isPlaying() const {
    return player.isPlaying();
}

void VideoSource::setFrame(int frame) {
    player.setFrame(frame);
}

void VideoSource::setPosition(float pct) {
    player.setPosition(pct);
    if (_hasAudio) {
        player.flushAudioBuffers();
    }
}

int VideoSource::getCurrentFrame() const {
    return player.getCurrentFrame();
}

int VideoSource::getTotalFrames() const {
    return player.getTotalNumFrames();
}

void VideoSource::setLoop(bool shouldLoop) {
    loop = shouldLoop;
    player.setLoopState(shouldLoop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
}

ofJson VideoSource::serialize() const {
    ofJson j;
    ofSerialize(j, *parameters);
    return j;
}

void VideoSource::deserialize(const ofJson& json) {
    ofJson j = json;
    if (j.contains("group")) {
        j = j["group"];
    } else if (j.contains("params")) {
        j = j["params"];
    }

    std::string pathValue;
    if (j.contains("videoPath")) {
        pathValue = getSafeJson<std::string>(j, "videoPath", "");
    } else if (j.contains("path")) {
        pathValue = getSafeJson<std::string>(j, "path", "");
    }
    if (!pathValue.empty()) path.set(pathValue);

    if (j.contains("loop")) loop = getSafeJson<bool>(j, "loop", loop.get());
    if (j.contains("speed")) speed = getSafeJson<float>(j, "speed", speed.get());
    if (j.contains("playOnLoad")) playing = getSafeJson<bool>(j, "playOnLoad", playing.get());
    if (j.contains("playing")) playing = getSafeJson<bool>(j, "playing", playing.get());

    ofDeserialize(j, *parameters);
}
