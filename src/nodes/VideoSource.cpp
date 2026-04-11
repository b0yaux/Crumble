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
        currentCycle = cycle;
        
        ofxHapPlayer* p = playerRef.load(std::memory_order_relaxed);
        if (!p || !p->isLoaded()) return;

        if (evalSlot(activeSlot, cycle) < 0.5f) {
            if (!p->isPaused()) p->setPaused(true);
            return;
        }

        float newSpeed = evalSlot(speedSlot, cycle);
        bool isPlaying = evalSlot(playingSlot, cycle) > 0.5f;
        bool isExternal = evalSlot(clockModeSlot, cycle) > 0.5f;

        if (isExternal) {
            if (!p->isPaused()) p->setPaused(true);
        } else {
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

        // Note: p->update() is NOT called here. ofxHapPlayer auto-updates
        // via ofEvents().update listener (priority 200, after ofApp::update).
        // The player's timeout is set to 0 (non-blocking) so seeks show the
        // previous frame for 1 frame rather than stalling up to 30ms.
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

    // Reduced fetch timeout: the default 30ms can cause visible stalls.
    // 5ms gives the demuxer time to deliver on most SSDs while keeping
    // the frame responsive for EXTERNAL-mode position updates.
    localPlayer.setTimeout(5000);

    path.addListener(this, &VideoSource::onPathChanged);
    clockMode.addListener(this, &VideoSource::onClockModeChanged);
    position.addListener(this, &VideoSource::onPositionChanged);
}

VideoSource::~VideoSource() {
    path.removeListener(this, &VideoSource::onPathChanged);
    clockMode.removeListener(this, &VideoSource::onClockModeChanged);
    position.removeListener(this, &VideoSource::onPositionChanged);
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
    int total = getTotalFrames();
    if (total > 0) {
        int frame = static_cast<int>(v * (total - 1));
        if (frame != lastSetFrame) {
            setPosition(v);
            lastSetFrame = frame;
        }
    } else {
        setPosition(v);
    }
}

void VideoSource::onPathChanged(std::string& p) {
    // Dedup against last PARAMETER path (not pattern-loaded path).
    // The path pattern overrides loadedPath with bank:index values,
    // so comparing against loadedPath would fail dedup on reload.
    if (!p.empty() && p != lastParamPath) {
        lastParamPath = p;
        if (bank.get().empty()) bank.set(Node::validBank(p));
        load(p);
    }
}


void VideoSource::load(const std::string& vidPath) {
    std::string resolvedPath = resolvePath(vidPath, "video");

    if (resolvedPath.empty()) return;
    
    bool isSameVideo = (loadedResolvedPath == resolvedPath);
    if (isSameVideo && getPlayer().isLoaded()) {
        if (clockMode == INTERNAL) {
            setPosition(0.0f);
            if (playing.get()) {
                getPlayer().play();
            }
        }
        return;
    }

    // Return current player to pool for reuse on future swaps.
    // The player stays alive with its decoded texture; next time we
    // need this video, we skip the blocking VideoCache::acquire().
    if (currentPlayer && currentPlayer->isLoaded() && !loadedResolvedPath.empty()) {
        if (playerPool.size() >= MAX_POOL_SIZE) {
            playerPool.erase(playerPool.begin());
        }
        currentPlayer->setPaused(true);
        playerPool[loadedResolvedPath] = {std::move(currentPlayer), _hasAudio};
        currentPlayer = nullptr;
    }

    // Try pool first (instant), then fall back to VideoCache::acquire() (blocking first load).
    auto poolIt = playerPool.find(resolvedPath);
    if (poolIt != playerPool.end() && poolIt->second.player && poolIt->second.player->isLoaded()) {
        currentPlayer = std::move(poolIt->second.player);
        _hasAudio = poolIt->second.hasAudio;
        playerPool.erase(poolIt);
        ofLogVerbose("VideoSource") << "Reused pooled player: " << resolvedPath;
    } else {
        auto cached = VideoCache::get().acquire(resolvedPath);
        if (!cached || !cached->loaded) {
            ofLogError("VideoSource") << "Failed to acquire video from cache: " << resolvedPath;
            _hasAudio = false;
            return;
        }
        currentPlayer = std::move(cached->player);
        _hasAudio = cached->hasAudio;
    }

    currentPlayer->setTimeout(5000);
    if (videoProcessor) {
        static_cast<VideoSourceProcessor*>(videoProcessor)->setPlayer(currentPlayer.get());
    }

    getPlayer().setLoopState(loop ? OF_LOOP_NORMAL : OF_LOOP_NONE);
    
    if (clockMode == VideoSource::INTERNAL) {
        if (playing) getPlayer().play();
        safeSetPlayerSpeed(speed.get());
    } else {
        getPlayer().setPaused(true);
    }
    
    loadedPath = vidPath;
    loadedResolvedPath = resolvedPath;
    lastSetFrame = -1;
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
            lastTriggerBars = std::floor(lastCtx.cycle);
        }
        
        if (std::abs(lastCtx.cycle - lastTriggerBars) > 0.5) {
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
