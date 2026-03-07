#include "ofMain.h"
#include "VideoFileSource.h"
#include "../core/NodeProcessor.h"

// Shadow processor for the video thread
class VideoFileProcessor : public crumble::VideoProcessor {
public:
    VideoFileProcessor(ofxHapPlayer* p) : playerRef(p), lastSpeed(1.0f) {}

    void processVideo(double cycle, double cycleStep) override {
        if (!playerRef || !playerRef->isLoaded()) return;

        float newSpeed = getParam("speed");

        // Only call into the player when the speed value actually changes.
        // Every setSpeed() call notifies the AudioThread unconditionally, and
        // setSpeed(0.0f) is unsafe: Clock::setRateAt stores _rate=0 then calls
        // syncAt() which computes pos/0.0f = +inf, casts it to int64_t (UB),
        // corrupting _start. On the next update() the clock returns a wrong pts,
        // causing a cache miss that blocks the main thread for up to 30 ms
        // (the player's _timeout). Use setPaused() for the zero crossing instead.
        if (std::abs(newSpeed - lastSpeed) > 1e-4f) {
            if (newSpeed == 0.0f) {
                playerRef->setPaused(true);
            } else {
                // Only un-pause for speed recovery if the node is actually meant to
                // be playing; don't override an explicit playing=false pause.
                if (lastSpeed == 0.0f && getParam("playing") > 0.5f) {
                    playerRef->setPaused(false);
                }
                playerRef->setSpeed(newSpeed);
            }
            lastSpeed = newSpeed;
        }

        playerRef->update();
    }
    
    ofTexture* getOutput(int index = 0) override {
        if (getParam("active") < 0.5f) return nullptr;
        
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

VideoFileSource::VideoFileSource() {
    type = "VideoFileSource";

    parameters->add(path.set("path", ""));
    parameters->add(loop.set("loop", true));
    parameters->add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters->add(playing.set("playing", true));
    parameters->add(clockMode.set("clockMode", VideoFileSource::INTERNAL, VideoFileSource::INTERNAL, VideoFileSource::EXTERNAL));

    path.addListener(this, &VideoFileSource::onPathChanged);
    clockMode.addListener(this, &VideoFileSource::onClockModeChanged);
}

crumble::VideoProcessor* VideoFileSource::createVideoProcessor() {
    return new VideoFileProcessor(&player);
}

void VideoFileSource::safeSetPlayerSpeed(float newSpeed) {
    // Never call setSpeed(0) on the HAP player: the Clock computes pos/rate
    // internally; rate==0 yields +inf which is cast to int64_t (UB), corrupting
    // _start and causing multi-ms stalls on the next update(). Route zero through
    // setPaused() instead, and restore the pause state on recovery.
    if (newSpeed == 0.0f) {
        player.setPaused(true);
    } else {
        // Un-pause only if we paused because speed reached zero AND the node is
        // supposed to be playing — don't override an explicit playing=false.
        if (lastSpeed == 0.0f && playing.get()) {
            player.setPaused(false);
        }
        player.setSpeed(newSpeed);
    }
    lastSpeed = newSpeed;
}

void VideoFileSource::onClockModeChanged(int& mode) {
    if (player.isLoaded()) {
        if (mode == VideoFileSource::EXTERNAL) {
            // Switching to external clock: freeze the player.
            // Do NOT call setSpeed(0) — use setPaused() to avoid HAP Clock UB.
            player.setPaused(true);
        } else {
            if (playing) {
                player.play();
                safeSetPlayerSpeed(speed.get());
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
            // play() must be called before setSpeed() so the HAP clock is
            // initialised with a non-zero rate from the start.
            if (playing) {
                player.play();
            }
            safeSetPlayerSpeed(speed.get());
        } else {
            // EXTERNAL mode relies entirely on manual scrubbing; just freeze.
            player.setPaused(true);
        }
        loadedPath = vidPath;
    } else {
        ofLogError("VideoFileSource") << "Failed to load: " << resolvedPath;
    }
}

void VideoFileSource::onParameterChanged(const std::string& paramName) {
    if (!player.isLoaded()) return;

    if (paramName == "speed") {
        if (clockMode == VideoFileSource::INTERNAL) {
            safeSetPlayerSpeed(speed.get());
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
    ofSerialize(j, *parameters);
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
