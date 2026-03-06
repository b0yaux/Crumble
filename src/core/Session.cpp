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
    while (commandQueue.try_dequeue(cmd)) {
        // Helper: is this processor still alive?
        auto alive = [&](crumble::NodeProcessor* p) -> bool {
            if (!p) return false;
            return std::find(activeProcessors.begin(), activeProcessors.end(), p)
                   != activeProcessors.end();
        };

        switch (cmd.type) {
            case crumble::AudioCommand::ADD_NODE:
                if (cmd.processor) {
                    activeProcessors.push_back(cmd.processor);
                }
                break;

            case crumble::AudioCommand::REMOVE_NODE:
                if (cmd.processor) {
                    auto it = std::find(activeProcessors.begin(), activeProcessors.end(), cmd.processor);
                    if (it != activeProcessors.end()) {
                        activeProcessors.erase(it);
                    }
                    // Release all pattern shared_ptrs so Pattern objects can be freed.
                    cmd.processor->patternMap.clear();
                    releaseQueue.enqueue(cmd.processor);
                }
                break;

            case crumble::AudioCommand::SET_PARAM:
                if (alive(cmd.processor) && !cmd.slotName.empty()) {
                    cmd.processor->valuesMap[cmd.slotName].store(cmd.value, std::memory_order_relaxed);
                }
                break;

            case crumble::AudioCommand::SET_PATTERN:
                // Install the Pattern object. If the processor is already gone the
                // shared_ptr simply destructs here — no leak, no crash.
                if (alive(cmd.processor) && !cmd.slotName.empty()) {
                    if (cmd.pattern) {
                        cmd.processor->patternMap[cmd.slotName] = cmd.pattern;
                    } else {
                        cmd.processor->patternMap.erase(cmd.slotName);
                    }
                }
                // cmd.pattern destructs cleanly whether we used it or not.
                break;

            case crumble::AudioCommand::CONNECT_NODES:
                if (cmd.targetProcessor) {
                    cmd.targetProcessor->addInput(cmd.processor, cmd.toInput, cmd.fromOutput);
                }
                break;

            case crumble::AudioCommand::DISCONNECT_NODES:
                if (cmd.targetProcessor) {
                    cmd.targetProcessor->removeInput(cmd.toInput);
                }
                break;

            case crumble::AudioCommand::LOAD_BUFFER:
                if (alive(cmd.processor)) {
                    cmd.processor->handleCommand(cmd);
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

    // 4. Compute cycle timing for this block so processors can evaluate patterns
    //    sample-accurately. transport.cycle is already updated for this block's start.
    double blockCycle = transport.cycle;
    double cycleStep  = transport.getCyclesPerSample(buffer.getSampleRate());

    // 5. Wait-Free DSP Traversal — pull from all sinks
    int sinkCount = 0;
    for (auto* processor : activeProcessors) {
        if (processor->isSink) {
            processor->pull(buffer, 0, frameCounter, blockCycle, cycleStep);
            sinkCount++;
        }
    }
    
    static int frameDebug = 0;
    if (frameDebug++ % 500 == 0) {
        ofLogNotice("Session") << "Audio thread: " << activeProcessors.size() << " processors, " << sinkCount << " sinks";
    }
}

void Session::sendCommand(const crumble::AudioCommand& cmd) {
    if (!commandQueue.enqueue(cmd)) {
        ofLogError("Session") << "Command Queue Overflow!";
    }
}

void Session::update(float dt) {
    // 1. Clean up released processors
    crumble::NodeProcessor* released = nullptr;
    while (releaseQueue.try_dequeue(released)) {
        delete released;
    }

    // 2. Prepare all nodes recursively for UI/Video
    Context ctx;
    ctx.cycle = transport.cycle;
    ctx.cycleStep = 0; 
    ctx.frames = 1;
    ctx.dt = dt;
    
    // NO LOCK NEEDED HERE - Nodes update their own local state
    // Audio thread reads from its own 'Shadow' state
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
            if (!node->touched) {
                graph.removeNode(nodeId);
            }
        }
    }
}

void Session::touchNode(int nodeId) {
    if (auto node = graph.getNode(nodeId)) {
        node->touched = true;
    }
}

Node* Session::getNode(int nodeId) {
    return graph.getNode(nodeId);
}

Node* Session::findNodeByName(const std::string& name) {
    for (const auto& [id, node] : graph.getNodes()) {
        if (node->name == name) return node.get();
    }
    return nullptr;
}

int Session::getNodeCount() const {
    return static_cast<int>(graph.getNodeCount());
}

bool Session::save(const std::string& path) {
    return graph.saveToFile(path);
}

bool Session::load(const std::string& path) {
    return graph.loadFromFile(path);
}

void Session::registerNodeType(const std::string& type, Graph::NodeCreator creator) {
    graph.registerNodeType(type, creator);
}

std::vector<std::string> Session::getRegisteredTypes() const {
    return graph.getRegisteredTypes();
}
