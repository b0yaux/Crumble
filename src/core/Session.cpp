#include "ofMain.h"
#include "Session.h"
#include "NodeProcessor.h"
#include "../nodes/SpeakersOutput.h"
#include <algorithm>

Session* g_session = nullptr;

Session::Session() {
    g_session = this;
    graph.setTransport(&transport);
    setupAudio(44100, 256); 
}

Session::~Session() {
    // 1. Explicitly destroy the graph while the audio thread is still running.
    //    Node::~Node() enqueues REMOVE_NODE commands — they must land while
    //    audioOut() can still dequeue them, otherwise processors are abandoned.
    graph.clear();

    // 2. Now it is safe to stop the stream.
    soundStream.stop();
    soundStream.close();

    // 3. Drain and delete every shadow processor that is still alive.
    //    activeAudioProcessors holds processors that were ADD_NODE'd but not yet
    //    REMOVE_NODE'd (e.g. nodes added after the last audioOut() call).
    for (auto* p : activeAudioProcessors) delete p;
    activeAudioProcessors.clear();

    for (auto* p : activeVideoProcessors) delete p;
    activeVideoProcessors.clear();

    // 4. Drain the deferred-release queues (processors removed during this run).
    crumble::AudioProcessor* ap = nullptr;
    while (audioReleaseQueue.try_dequeue(ap)) delete ap;

    crumble::VideoProcessor* vp = nullptr;
    while (videoReleaseQueue.try_dequeue(vp)) delete vp;

    // 5. Drain patterns that were displaced from the audio thread.
    std::shared_ptr<Pattern<float>> pat;
    while (patternReleaseQueue.try_dequeue(pat)) pat.reset();

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
    crumble::ProcessorCommand cmd;
    while (audioCommandQueue.try_dequeue(cmd)) {
        // Helper: is this processor still alive? O(1) with unordered_set.
        auto alive = [&](crumble::AudioProcessor* p) -> bool {
            return p && activeAudioProcessors.count(p) > 0;
        };

        crumble::AudioProcessor* ap = cmd.audioProcessor;

        switch (cmd.type) {
            case crumble::ProcessorCommand::ADD_NODE:
                if (ap) {
                    activeAudioProcessors.insert(ap);
                }
                break;

            case crumble::ProcessorCommand::REMOVE_NODE:
                if (ap) {
                    if (activeAudioProcessors.erase(ap) > 0) {
                        // Also remove from endpoint list if it was registered as one
                        auto eit = std::find(audioEndpoints.begin(), audioEndpoints.end(), ap);
                        if (eit != audioEndpoints.end()) {
                            audioEndpoints.erase(eit);
                        }
                        
                        ap->patternMap.clear();
                        audioReleaseQueue.enqueue(ap);
                    }
                }
                break;

            case crumble::ProcessorCommand::SET_PARAM:
                if (alive(ap) && ap->nodeId == cmd.nodeId && !cmd.slotName.empty()) {
                    ap->valuesMap[cmd.slotName].store(cmd.value, std::memory_order_relaxed);
                }
                break;

            case crumble::ProcessorCommand::SET_PATTERN:
                if (alive(ap) && ap->nodeId == cmd.nodeId && !cmd.slotName.empty()) {
                    if (cmd.pattern) {
                        // Move any displaced pattern to the release queue so its
                        // destructor runs on the main thread, not the audio thread.
                        auto it = ap->patternMap.find(cmd.slotName);
                        if (it != ap->patternMap.end() && it->second) {
                            patternReleaseQueue.enqueue(std::move(it->second));
                        }
                        ap->patternMap[cmd.slotName] = cmd.pattern;
                    } else {
                        auto it = ap->patternMap.find(cmd.slotName);
                        if (it != ap->patternMap.end() && it->second) {
                            patternReleaseQueue.enqueue(std::move(it->second));
                        }
                        ap->patternMap.erase(cmd.slotName);
                    }
                }
                break;

            case crumble::ProcessorCommand::CONNECT_NODES:
                if (alive(cmd.targetAudioProcessor) && (ap == nullptr || alive(ap))) {
                    // Verify IDs if processors are present
                    bool targetMatch = (cmd.targetAudioProcessor->nodeId == cmd.targetId);
                    bool sourceMatch = (ap == nullptr || ap->nodeId == cmd.nodeId);
                    
                    if (targetMatch && sourceMatch) {
                        cmd.targetAudioProcessor->addInput(ap, cmd.toInput, cmd.fromOutput);
                    }
                }
                break;

            case crumble::ProcessorCommand::DISCONNECT_NODES:
                if (alive(cmd.targetAudioProcessor) && cmd.targetAudioProcessor->nodeId == cmd.targetId) {
                    cmd.targetAudioProcessor->removeInput(cmd.toInput);
                }
                break;

            case crumble::ProcessorCommand::LOAD_BUFFER:
                if (alive(ap)) ap->handleCommand(cmd);
                break;

            case crumble::ProcessorCommand::RELEASE_BUFFER:
                // Zero the processor's data pointer and release its dataOwner
                // reference before REMOVE_NODE arrives.  Safe to call even if
                // the processor has already been removed (alive() guards it).
                if (alive(ap)) ap->handleCommand(cmd);
                break;

            case crumble::ProcessorCommand::REGISTER_ENDPOINT:
                // Nominate this processor as a session-driven audio endpoint.
                // Duplicates are guarded: a node rebuilt via hot-reload will
                // send ADD_NODE + REGISTER_ENDPOINT again, but the old processor
                // will have been REMOVE_NODE'd first, so this list stays clean.
                if (ap) {
                    if (std::find(audioEndpoints.begin(), audioEndpoints.end(), ap)
                            == audioEndpoints.end()) {
                        audioEndpoints.push_back(ap);
                    }
                }
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

    // 4. Compute cycle timing for this block (monotonically increasing bars)
    double blockBars = transport.bars;
    double barsStep  = transport.getCyclesPerSample(buffer.getSampleRate());

    // 5. Wait-Free DSP Traversal — pull from every registered audio endpoint.
    // Endpoints register themselves via REGISTER_ENDPOINT (e.g. SpeakersOutput).
    // The list is maintained exclusively on this thread so no lock is needed.
    static int lastProcessorCount = 0;
    if ((int)activeAudioProcessors.size() != lastProcessorCount) {
        ofLogNotice("Session") << "Shadow audio processors registered: " << activeAudioProcessors.size();
        ofLogNotice("Session") << "Hardware callback drivers (audio endpoints): " << audioEndpoints.size();
        lastProcessorCount = activeAudioProcessors.size();
    }
    for (auto* ep : audioEndpoints) {
        ep->pull(buffer, 0, frameCounter, blockBars, barsStep);
    }
}

void Session::registerAudioEndpoint(crumble::AudioProcessor* ap) {
    if (!ap) return;
    crumble::ProcessorCommand cmd;
    cmd.type = crumble::ProcessorCommand::REGISTER_ENDPOINT;
    cmd.audioProcessor = ap;
    if (!audioCommandQueue.enqueue(cmd)) {
        ofLogError("Session") << "Audio Command Queue Overflow on REGISTER_ENDPOINT!";
    }
}

void Session::sendCommand(const crumble::ProcessorCommand& cmd) {
    bool hasAudio = cmd.audioProcessor || cmd.targetAudioProcessor;
    bool hasVideo = cmd.videoProcessor || cmd.targetVideoProcessor;
    
    if (hasAudio) {
        if (!audioCommandQueue.enqueue(cmd)) {
            ofLogError("Session") << "Audio Command Queue Overflow!";
        }
    }
    if (hasVideo) {
        if (!videoCommandQueue.enqueue(cmd)) {
            ofLogError("Session") << "Video Command Queue Overflow!";
        }
    }
}

void Session::update(float dt) {
    // 1. Process Video Commands (on Main Thread)
    crumble::ProcessorCommand cmd;
    while (videoCommandQueue.try_dequeue(cmd)) {
        // Helper: is this processor still alive? O(1) with unordered_set.
        auto alive = [&](crumble::VideoProcessor* p) -> bool {
            return p && activeVideoProcessors.count(p) > 0;
        };

        crumble::VideoProcessor* vp = cmd.videoProcessor;
        
        switch (cmd.type) {
            case crumble::ProcessorCommand::ADD_NODE:
                if (vp) {
                    activeVideoProcessors.insert(vp);
                }
                break;

            case crumble::ProcessorCommand::REMOVE_NODE:
                if (vp) {
                    if (activeVideoProcessors.erase(vp) > 0) {
                        vp->patternMap.clear();
                        videoReleaseQueue.enqueue(vp);
                    }
                }
                break;

            case crumble::ProcessorCommand::SET_PARAM:
                if (alive(vp) && vp->nodeId == cmd.nodeId && !cmd.slotName.empty()) {
                    vp->valuesMap[cmd.slotName].store(cmd.value, std::memory_order_relaxed);
                }
                break;

            case crumble::ProcessorCommand::SET_PATTERN:
                if (alive(vp) && vp->nodeId == cmd.nodeId && !cmd.slotName.empty()) {
                    if (cmd.pattern) {
                        vp->patternMap[cmd.slotName] = cmd.pattern;
                    } else {
                        vp->patternMap.erase(cmd.slotName);
                    }
                }
                break;

            case crumble::ProcessorCommand::CONNECT_NODES:
                if (alive(cmd.targetVideoProcessor) && (vp == nullptr || alive(vp))) {
                    // Verify IDs if processors are present
                    bool targetMatch = (cmd.targetVideoProcessor->nodeId == cmd.targetId);
                    bool sourceMatch = (vp == nullptr || vp->nodeId == cmd.nodeId);
                    
                    if (targetMatch && sourceMatch) {
                        cmd.targetVideoProcessor->addInput(vp, cmd.toInput, cmd.fromOutput);
                    }
                }
                break;

            case crumble::ProcessorCommand::DISCONNECT_NODES:
                if (alive(cmd.targetVideoProcessor) && cmd.targetVideoProcessor->nodeId == cmd.targetId) {
                    cmd.targetVideoProcessor->removeInput(cmd.toInput);
                }
                break;

            case crumble::ProcessorCommand::LOAD_BUFFER:
                if (alive(vp)) vp->handleCommand(cmd);
                break;

            default: break;
        }
    }

    // 2. Clean up released processors
    // processors are dequeued and deleted. These are already removed from 
    // the active lists by the shadow threads (audioOut/update).
    crumble::AudioProcessor* releasedAudio = nullptr;
    while (audioReleaseQueue.try_dequeue(releasedAudio)) {
        delete releasedAudio;
    }

    // Drain patterns displaced from the audio thread — destructors run here
    // on the main thread, never inside the real-time callback.
    std::shared_ptr<Pattern<float>> releasedPattern;
    while (patternReleaseQueue.try_dequeue(releasedPattern)) {
        releasedPattern.reset();
    }

    crumble::VideoProcessor* releasedVideo = nullptr;
    while (videoReleaseQueue.try_dequeue(releasedVideo)) {
        delete releasedVideo;
    }

    // 3. Evaluate Video Processors (Shadow Graph)
    double blockBars = transport.bars;
    double barsStep = 0.0; // Pattern interp on video thread is K-rate (once per frame)
    
    static int lastVideoProcessorCount = 0;
    if ((int)activeVideoProcessors.size() != lastVideoProcessorCount) {
        ofLogNotice("Session") << "Shadow video processors registered: " << activeVideoProcessors.size();
        lastVideoProcessorCount = activeVideoProcessors.size();
    }
    
    for (auto* vp : activeVideoProcessors) {
        vp->currentCycle = blockBars; // Update cycle for pattern-aware getParam()
        vp->processVideo(blockBars, barsStep);
    }

    // 4. Update UI Graph
    Context ctx;
    ctx.cycle = blockBars;
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
    int removedCount = 0;
    for (int nodeId : nodeIds) {
        if (auto node = graph.getNode(nodeId)) {
            if (!node->touched) {
                ofLogNotice("Session") << "Removing untouted node: " << node->name << " (id=" << nodeId << ")";
                graph.removeNode(nodeId);
                removedCount++;
            }
        }
    }
    ofLogNotice("Session") << "endScript: removed " << removedCount << " nodes, remaining: " << graph.getNodeCount();
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