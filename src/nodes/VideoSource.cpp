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
    crumble::ControlSlot* clockModeSlot = nullptr;

    VideoSourceProcessor(ofxHapPlayer* p) : lastSpeed(1.0f) {
        playerRef.store(p);
        speedSlot = getControlPtr(crumble::hashString("speed"));
        playingSlot = getControlPtr(crumble::hashString("playing"));
        activeSlot = getControlPtr(crumble::hashString("active"));
        clockModeSlot = getControlPtr(crumble::hashString("clockMode"));
    }

    void processVideo(double cycle, double cycleStep) override {
        currentCycle = cycle; // Update cycle for pattern-aware getParam()
        
        ofxHapPlayer* p = playerRef.load(std::memory_order_relaxed);
        if (!p || !p->isLoaded()) return;

        // Active parameter controls global bypass.
        if (evalSlot(activeSlot, cycle) < 0.5f) {
            if (!p->isPaused()) p->setPaused(true);
            return;
        }

        float newSpeed = evalSlot(speedSlot, cycle);
        bool isPlaying = evalSlot(playingSlot, cycle) > 0.5f;
        bool isExternal = evalSlot(clockModeSlot, cycle) > 0.5f;

        // In slaved mode (clockMode=EXTERNAL), we keep the player paused to prevent its internal 
        // timer from advancing. The frame is updated manually via setPosition() calls 
        // issued from the main thread (e.g. slaved to an audio playhead).
        if (isExternal) {
            if (!p->isPaused()) p->setPaused(true);
        } else {
            // INTERNAL clock mode: manage pause state based on speed and playing param
            if (!isPlaying || newSpeed == 0.0f) {
                if (!p->isPaused()) p->setPaused(true);
                lastSpeed = newSpeed;
            } else {
                if (std::abs(newSpeed - lastSpeed) > 1e-4f || p->isPaused()) {
                    if (p->isPaused()) p->setPaused(false);
                    p->setSpeed(newSpeed);
                    lastSpeed = newSpeed;
                }
            }
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
    parameters->add(bank.set("bank", ""));
    parameters->add(loop.set("loop", true));
    parameters->add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters->add(position.set("position", 0.0, 0.0, 1.0));
    parameters->add(playing.set("playing", true));
    parameters->add(clockMode.set("clockMode", VideoSource::INTERNAL, VideoSource::INTERNAL, VideoSource::EXTERNAL));

    path.addListener(this, &VideoSource::onPathChanged);
    clockMode.addListener(this, &VideoSource::onClockModeChanged);
    position.addListener(this, &VideoSource::onPositionChanged);
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

void VideoSource::onPositionChanged(float& v) {
    setPosition(v);
}

void VideoSource::onPathChanged(std::string& p) {
    // Dedup against last PARAMETER path (not pattern-loaded path).
    // The path pattern overrides loadedPath with bank:index values,
    // so comparing against loadedPath would fail dedup on reload.
    if (!p.empty() && p != lastParamPath) {
        lastParamPath = p;
        if (bank.get().empty()) bank.set(Node::extractBank(p));
        load(p);
    }
}


void VideoSource::load(const std::string& vidPath) {
    std::string resolvedPath = resolvePath(vidPath, "video");
    
    bool isSameVideo = (loadedResolvedPath == resolvedPath);
    if (isSameVideo && getPlayer().isLoaded()) {
        // Same video reload, just seek to start.
        // Even if skip reload, must reset position for path patterns (e.g. travaux ~)
        setPosition(0.0f);
        if (clockMode == INTERNAL && playing.get()) {
            getPlayer().play();
        }
        return;
    }

    // Acquire from global cache (or load if not cached)
    auto cached = VideoCache::get().acquire(resolvedPath);
    if (!cached || !cached->loaded) {
        ofLogError("VideoSource") << "Failed to acquire video from cache: " << resolvedPath;
        _hasAudio = false;
        return;
    }
    
    // Different video - swap in the cached player
    ofLogVerbose("VideoSource") << "Swapping to cached video: " << resolvedPath;
    
    // Stop current playback
    if (getPlayer().isLoaded()) {
        getPlayer().stop();
    }
    
    // Swap pointers and update processor
    currentPlayer = std::move(cached->player);
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
    Node::onParameterChanged(paramName);

    if (!getPlayer().isLoaded()) return;

    if (paramName == "speed" || paramName == "loop" || paramName == "playing") {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::SET_PARAM;
        cmd.nodeId = nodeId;
        cmd.paramHash = crumble::hashString(paramName.c_str());
        cmd.videoProcessor = videoProcessor;
        
        float vVal = 0;
        if (paramName == "speed") vVal = speed.get();
        else if (paramName == "loop") vVal = loop.get() ? 1.0f : 0.0f;
        else if (paramName == "playing") vVal = playing.get() ? 1.0f : 0.0f;
        
        cmd.value = vVal;
        pushCommand(cmd);

        if (paramName == "speed" && clockMode == VideoSource::INTERNAL) {
            safeSetPlayerSpeed(speed.get());
        } else if (paramName == "loop") {
            getPlayer().setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
        } else if (paramName == "playing" && clockMode == VideoSource::INTERNAL) {
            if (playing) getPlayer().play();
            else getPlayer().setPaused(true);
        }
    }
}

void VideoSource::update(float dt) {
    auto pat = getPattern("path");
    if (pat) {
        if (lastTriggerBars < 0) {
            // On first run or reload, catch triggers from the start of the current bar
            lastTriggerBars = std::floor(lastCtx.cycle);
        }
        
        if (std::abs(lastCtx.cycle - lastTriggerBars) > 0.5) {
            // Cycle wrap-around
            lastTriggerBars = std::floor(lastCtx.cycle);
        }

        double start = lastTriggerBars;
        double end = lastCtx.cycle + lastCtx.cycleStep;
        auto events = pat->query(start, end);
        
        for (const auto& e : events) {
            if (e.isRest) continue;
            
            if (e.ref) {
                load(*(e.ref));
            } else {
                int idx = static_cast<int>(std::floor(e.value));
                std::string b = bank.get();
                if (!b.empty()) {
                    load(b + ":" + std::to_string(idx));
                }
            }
        }
        lastTriggerBars = end;
    }

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
