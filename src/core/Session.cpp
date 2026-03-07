#include "ofMain.h"
#include "Session.h"
#include "NodeProcessor.h"
#include "../nodes/SpeakersOutput.h"
#include <algorithm>

Session* g_session = nullptr;

Session::Session() {
    g_session = this;
    setupAudio(44100, 256); 
}

Session::~Session() {
    soundStream.stop();
    soundStream.close();
    g_session = nullptr;
}

void Session::setupAudio(int sampleRate, int bufferSize) {
    ofLogNotice("Session") << "Initializing Master Audio Engine: " << sampleRate << "Hz, " << bufferSize << " samples";
    soundStream.stop();
    soundStream.close();

    ofSoundStreamSettings settings;
    settings.setOutListener(this);
    settings.sampleRate = sampleRate;
    settings.bufferSize = bufferSize;
    settings.numOutputChannels = 2;
    settings.numInputChannels = 0;

    bool success = soundStream.setup(settings);
    if (success) {
        ofLogNotice("Session") << "Master Audio stream started successfully.";
    } else {
        ofLogError("Session") << "FAILED to start master audio stream.";
    }
}

void Session::audioOut(ofSoundBuffer& buffer) {
    // 1. Process all pending commands (Wait-Free)
    crumble::AudioCommand cmd;
    while (audioCommandQueue.try_dequeue(cmd)) {
        // Helper: is this processor still alive?
        auto alive = [&](crumble::AudioProcessor* p) -> bool {
            if (!p) return false;
            return std::find(activeAudioProcessors.begin(), activeAudioProcessors.end(), p)
                   != activeAudioProcessors.end();
        };

        crumble::AudioProcessor* ap = cmd.audioProcessor;

        switch (cmd.type) {
            case crumble::AudioCommand::ADD_NODE:
                if (ap) activeAudioProcessors.push_back(ap);
                break;

            case crumble::AudioCommand::REMOVE_NODE:
                if (ap) {
                    auto it = std::find(activeAudioProcessors.begin(), activeAudioProcessors.end(), ap);
                    if (it != activeAudioProcessors.end()) {
                        activeAudioProcessors.erase(it);
                    }
                    ap->patternMap.clear();
                    audioReleaseQueue.enqueue(ap);
                }
                break;

            case crumble::AudioCommand::SET_PARAM:
                if (alive(ap) && !cmd.slotName.empty()) {
                    ap->valuesMap[cmd.slotName].store(cmd.value, std::memory_order_relaxed);
                }
                break;

            case crumble::AudioCommand::SET_PATTERN:
                if (alive(ap) && !cmd.slotName.empty()) {
                    if (cmd.pattern) {
                        ap->patternMap[cmd.slotName] = cmd.pattern;
                    } else {
                        ap->patternMap.erase(cmd.slotName);
                    }
                }
                break;

            case crumble::AudioCommand::CONNECT_NODES:
                if (cmd.targetAudioProcessor) {
                    cmd.targetAudioProcessor->addInput(ap, cmd.toInput, cmd.fromOutput);
                }
                break;

            case crumble::AudioCommand::DISCONNECT_NODES:
                if (cmd.targetAudioProcessor) {
                    cmd.targetAudioProcessor->removeInput(cmd.toInput);
                }
                break;

            case crumble::AudioCommand::LOAD_BUFFER:
                if (alive(ap)) ap->handleCommand(cmd);
                break;

            default: break;
        }
    }

    // 2. Advance the absolute clock
    float dt = buffer.getNumFrames() / (float)buffer.getSampleRate();
    transport.update(dt);
    frameCounter++;
    
    // 3. Clear buffer initially
    buffer.set(0);

    // 4. Compute cycle timing for this block
    double blockCycle = transport.cycle;
    double cycleStep  = transport.getCyclesPerSample(buffer.getSampleRate());

    // 5. Wait-Free DSP Traversal
    for (auto* ap : activeAudioProcessors) {
        if (ap->isSink) {
            ap->pull(buffer, 0, frameCounter, blockCycle, cycleStep);
        }
    }
}

void Session::sendCommand(const crumble::AudioCommand& cmd) {
    if (cmd.audioProcessor) {
        if (!audioCommandQueue.enqueue(cmd)) {
            ofLogError("Session") << "Audio Command Queue Overflow!";
        }
    }
    if (cmd.videoProcessor) {
        if (!videoCommandQueue.enqueue(cmd)) {
            ofLogError("Session") << "Video Command Queue Overflow!";
        }
    }
    
    // Legacy fallback (remove once fully ported)
    if (cmd.processor && !cmd.audioProcessor && !cmd.videoProcessor) {
        ofLogWarning("Session") << "Legacy processor command sent! Type: " << cmd.type;
    }
}

void Session::update(float dt) {
    // 1. Process Video Commands (on Main Thread)
    crumble::AudioCommand cmd;
    while (videoCommandQueue.try_dequeue(cmd)) {
        auto alive = [&](crumble::VideoProcessor* p) -> bool {
            if (!p) return false;
            return std::find(activeVideoProcessors.begin(), activeVideoProcessors.end(), p)
                   != activeVideoProcessors.end();
        };

        crumble::VideoProcessor* vp = cmd.videoProcessor;
        
        switch (cmd.type) {
            case crumble::AudioCommand::ADD_NODE:
                if (vp) activeVideoProcessors.push_back(vp);
                break;

            case crumble::AudioCommand::REMOVE_NODE:
                if (vp) {
                    auto it = std::find(activeVideoProcessors.begin(), activeVideoProcessors.end(), vp);
                    if (it != activeVideoProcessors.end()) {
                        activeVideoProcessors.erase(it);
                    }
                    vp->patternMap.clear();
                    videoReleaseQueue.enqueue(vp);
                }
                break;

            case crumble::AudioCommand::SET_PARAM:
                if (alive(vp) && !cmd.slotName.empty()) {
                    vp->valuesMap[cmd.slotName].store(cmd.value, std::memory_order_relaxed);
                }
                break;

            case crumble::AudioCommand::SET_PATTERN:
                if (alive(vp) && !cmd.slotName.empty()) {
                    if (cmd.pattern) {
                        vp->patternMap[cmd.slotName] = cmd.pattern;
                    } else {
                        vp->patternMap.erase(cmd.slotName);
                    }
                }
                break;

            case crumble::AudioCommand::CONNECT_NODES:
                if (cmd.targetVideoProcessor) {
                    cmd.targetVideoProcessor->addInput(vp, cmd.toInput, cmd.fromOutput);
                }
                break;

            case crumble::AudioCommand::DISCONNECT_NODES:
                if (cmd.targetVideoProcessor) {
                    cmd.targetVideoProcessor->removeInput(cmd.toInput);
                }
                break;

            case crumble::AudioCommand::LOAD_BUFFER:
                if (alive(vp)) vp->handleCommand(cmd);
                break;

            default: break;
        }
    }

    // 2. Clean up released processors
    crumble::AudioProcessor* releasedAudio = nullptr;
    while (audioReleaseQueue.try_dequeue(releasedAudio)) delete releasedAudio;

    crumble::VideoProcessor* releasedVideo = nullptr;
    while (videoReleaseQueue.try_dequeue(releasedVideo)) delete releasedVideo;

    // 3. Evaluate Video Processors (Shadow Graph)
    double blockCycle = transport.cycle;
    double cycleStep = 0.0; // Pattern interp on video thread is K-rate (once per frame)
    for (auto* vp : activeVideoProcessors) {
        vp->processVideo(blockCycle, cycleStep);
    }

    // 4. Update UI Graph
    Context ctx;
    ctx.cycle = transport.cycle;
    ctx.cycleStep = 0; 
    ctx.frames = 1;
    ctx.dt = dt;
    
    graph.prepare(ctx);
    graph.update(dt);
}

void Session::draw() {
    graph.draw();
}

Node* Session::addNode(const std::string& type, const std::string& name) {
    return graph.createNode(type, name);
}

void Session::removeNode(int nodeId) {
    graph.removeNode(nodeId);
}

void Session::connect(int fromNode, int toNode, int fromOutput, int toInput) {
    graph.connect(fromNode, toNode, fromOutput, toInput);
}

void Session::disconnect(int toNode, int toInput) {
    graph.disconnect(toNode, toInput);
}

void Session::clear() {
    graph.clear();
}

void Session::beginScript() {
    for (auto& [nodeId, node] : graph.getNodes()) {
        node->touched = false;
    }
}

void Session::endScript() {
    std::vector<int> nodeIds;
    for (const auto& [id, node] : graph.getNodes()) {
        nodeIds.push_back(id);
    }
    for (int nodeId : nodeIds) {
        if (auto node = graph.getNode(nodeId)) {
            if (!node->touched) graph.removeNode(nodeId);
        }
    }
}

void Session::touchNode(int nodeId) {
    if (auto node = graph.getNode(nodeId)) node->touched = true;
}

Node* Session::getNode(int nodeId) { return graph.getNode(nodeId); }
Node* Session::findNodeByName(const std::string& name) {
    for (const auto& [id, node] : graph.getNodes()) {
        if (node->name == name) return node.get();
    }
    return nullptr;
}
int Session::getNodeCount() const { return static_cast<int>(graph.getNodeCount()); }
bool Session::save(const std::string& path) { return graph.saveToFile(path); }
bool Session::load(const std::string& path) { return graph.loadFromFile(path); }
void Session::registerNodeType(const std::string& type, Graph::NodeCreator creator) {
    graph.registerNodeType(type, creator);
}
std::vector<std::string> Session::getRegisteredTypes() const {
    return graph.getRegisteredTypes();
}