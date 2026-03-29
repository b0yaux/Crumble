#include "ofMain.h"
#include "VideoSource.h"
#include "../core/NodeProcessor.h"
#include "../core/VideoCache.h"

// Shadow processor for the video thread
class VideoSourceProcessor : public crumble::VideoProcessor {
public:
    crumble::ControlSlot* speedSlot = nullptr;
    crumble::ControlSlot* playingSlot = nullptr;
    crumble::ControlSlot* activeSlot = nullptr;

    VideoSourceProcessor(ofxHapPlayer* p) : lastSpeed(1.0f) {
        playerRef.store(p);
        speedSlot = getControlPtr(crumble::hashString("speed"));
        playingSlot = getControlPtr(crumble::hashString("playing"));
        activeSlot = getControlPtr(crumble::hashString("active"));
    }

    void processVideo(double cycle, double cycleStep) override {
        currentCycle = cycle; // Update cycle for pattern-aware getParam()
        
        ofxHapPlayer* p = playerRef.load(std::memory_order_relaxed);
        if (!p || !p->isLoaded()) return;

        float newSpeed = evalSlot(speedSlot, cycle);

        if (std::abs(newSpeed - lastSpeed) > 1e-4f) {
            if (newSpeed == 0.0f) {
                p->setPaused(true);
            } else {
                if (lastSpeed == 0.0f && evalSlot(playingSlot, cycle) > 0.5f) {
                    p->setPaused(false);
                }
                p->setSpeed(newSpeed);
            }
            lastSpeed = newSpeed;
        }

        p->update();
    }
    
    ofTexture* getOutput(int index = 0) override {
        if (evalSlot(activeSlot, currentCycle) < 0.5f) return nullptr;
        
        ofxHapPlayer* p = playerRef.load(std::memory_order_relaxed);
        if (p && p->isLoaded()) {
            ofTexture* tex = p->getTexture();
            if (tex && tex->isAllocated()) return tex;
        }
        return nullptr;
    }
    
    void setPlayer(ofxHapPlayer* p) {
        playerRef.store(p, std::memory_order_relaxed);
    }
    
private:
    std::atomic<ofxHapPlayer*> playerRef;
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
    return new VideoSourceProcessor(currentPlayer ? currentPlayer.get() : &localPlayer);
}

void VideoSource::setClockMode(ClockMode mode) {
    clockMode.set(static_cast<int>(mode));
}

void VideoSource::safeSetPlayerSpeed(float newSpeed) {
    if (newSpeed == 0.0f) {
        getPlayer().setPaused(true);
    } else {
        if (lastSpeed == 0.0f && playing.get()) {
            getPlayer().setPaused(false);
        }
        getPlayer().setSpeed(newSpeed);
    }
    lastSpeed = newSpeed;
}

void VideoSource::onClockModeChanged(int& mode) {
    if (getPlayer().isLoaded()) {
        if (mode == VideoSource::EXTERNAL) {
            getPlayer().setPaused(true);
        } else {
            if (playing) {
                getPlayer().play();
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
    
    // Acquire from global cache (or load if not cached)
    auto cached = VideoCache::get().acquire(resolvedPath);
    if (!cached) {
        ofLogError("VideoSource") << "Failed to acquire video from cache: " << resolvedPath;
        _hasAudio = false;
        return;
    }
    
    // Check if this is the same video already loaded in this player
    bool isSameVideo = (loadedResolvedPath == resolvedPath);
    
    if (isSameVideo && getPlayer().isLoaded()) {
        // Same video, just seek to start
        ofLogNotice("VideoSource") << "Same video reload, seeking to start: " << resolvedPath;
        setPosition(0.0f);
        if (clockMode == INTERNAL && playing.get()) {
            getPlayer().play();
        }
        return;
    }
    
    // Different video - swap in the cached player
    ofLogNotice("VideoSource") << "Swapping to cached video: " << resolvedPath;
    
    // Stop current playback
    if (getPlayer().isLoaded()) {
        getPlayer().stop();
    }
    
    // Swap pointers and update processor
    currentPlayer = cached->player;
    if (videoProcessor) {
        static_cast<VideoSourceProcessor*>(videoProcessor)->setPlayer(currentPlayer.get());
    }
    
    _hasAudio = cached->hasAudio;
    getPlayer().setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
    
    if (clockMode == VideoSource::INTERNAL) {
        if (playing) getPlayer().play();
        safeSetPlayerSpeed(speed.get());
    } else {
        getPlayer().setPaused(true);
    }
    
    loadedPath = vidPath;
    loadedResolvedPath = resolvedPath;
}

void VideoSource::onParameterChanged(const std::string& paramName) {
    if (!getPlayer().isLoaded()) return;

    if (paramName == "speed") {
        if (clockMode == VideoSource::INTERNAL) {
            safeSetPlayerSpeed(speed.get());
        }
    } else if (paramName == "loop") {
        getPlayer().setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
    } else if (paramName == "playing") {
        if (clockMode == VideoSource::INTERNAL) {
            if (playing) getPlayer().play();
            else getPlayer().setPaused(true);
        }
    }
}

void VideoSource::update(float dt) {
    if (getPlayer().isLoaded()) {
        getPlayer().update();
    }
}

ofTexture* VideoSource::processVideo(int index) {
    if (index != 0) return nullptr;
    if (getPlayer().isLoaded()) {
        ofTexture* tex = getPlayer().getTexture();
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
    getPlayer().play();
}

void VideoSource::stop() {
    getPlayer().stop();
}

void VideoSource::pause() {
    getPlayer().setPaused(true);
}

bool VideoSource::isPlaying() const {
    return getPlayer().isPlaying();
}

void VideoSource::setFrame(int frame) {
    getPlayer().setFrame(frame);
}

void VideoSource::setPosition(float pct) {
    getPlayer().setPosition(pct);
}

int VideoSource::getCurrentFrame() const {
    return getPlayer().getCurrentFrame();
}

int VideoSource::getTotalFrames() const {
    return getPlayer().getTotalNumFrames();
}

float VideoSource::getPosition() const {
    return getPlayer().getPosition();
}

void VideoSource::setLoop(bool shouldLoop) {
    loop = shouldLoop;
    getPlayer().setLoopState(shouldLoop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
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
