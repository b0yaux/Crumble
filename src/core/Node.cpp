#include "ofMain.h"
#include "Node.h"
#include "Graph.h"
#include "Session.h"
#include "NodeProcessor.h"
#include "ProcessorCommand.h"

std::atomic<int> Node::nextNodeId{0};

Node::Node() {
    parameters = std::make_shared<ofParameterGroup>();
    parameters->setName("parameters");
    
    volume = std::make_shared<ofParameter<float>>();
    parameters->add(volume->set("volume", 1.0, 0.0, 1.0));
    
    opacity = std::make_shared<ofParameter<float>>();
    parameters->add(opacity->set("opacity", 1.0, 0.0, 1.0));
    
    active = std::make_shared<ofParameter<bool>>();
    parameters->add(active->set("active", true));
    
    drawLayer = std::make_shared<ofParameter<int>>();
    parameters->add(drawLayer->set("drawLayer", 0, -100, 100));
}

Node::~Node() {
    if (g_session) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::REMOVE_NODE;
        cmd.audioProcessor = audioProcessor;
        cmd.videoProcessor = videoProcessor;
        g_session->sendCommand(cmd);
    }
}

void Node::setupProcessor() {
    if (audioProcessor || videoProcessor) return;

    audioProcessor = createAudioProcessor();
    videoProcessor = createVideoProcessor();

    auto initProcessor = [&](crumble::NodeProcessor* p) {
        if (!p) return;
        p->nodeId = nodeId;
        for (int i = 0; i < (int)parameters->size(); i++) {
            auto& param = parameters->get(i);
            float val = 0;
            bool supported = false;

            if (param.type() == typeid(ofParameter<float>).name()) {
                val = param.cast<float>().get();
                supported = true;
            } else if (param.type() == typeid(ofParameter<bool>).name()) {
                val = param.cast<bool>().get() ? 1.0f : 0.0f;
                supported = true;
            } else if (param.type() == typeid(ofParameter<int>).name()) {
                val = (float)param.cast<int>().get();
                supported = true;
            }

            if (supported) {
                p->valuesMap[param.getName()].store(val);
            }
        }
    };

    initProcessor(audioProcessor);
    initProcessor(videoProcessor);

    if (audioProcessor || videoProcessor) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::ADD_NODE;
        cmd.audioProcessor = audioProcessor;
        cmd.videoProcessor = videoProcessor;
        pushCommand(cmd);
    }
}

void Node::pushCommand(crumble::ProcessorCommand cmd) {
    if (g_session) {
        cmd.nodeId = nodeId;
        if (!cmd.audioProcessor) cmd.audioProcessor = audioProcessor;
        if (!cmd.videoProcessor) cmd.videoProcessor = videoProcessor;
        g_session->sendCommand(cmd);
    }
}

void Node::prepare(const Context& ctx) {
    if (lastPreparedCycle == ctx.cycle && ctx.frames > 1) return;
    lastPreparedCycle = ctx.cycle;

    std::lock_guard<std::recursive_mutex> lock(modMutex);

    // Update controlBuffers for UI/video use (getControl()).
    // Patterns are sent to the audio thread via onParameterChanged().
    for (auto& [paramName, pattern] : modulators) {
        if (!pattern) continue;

        auto& buf = controlBuffers[paramName];
        if (buf.getNumFrames() != (size_t)ctx.frames) {
            buf.allocate(ctx.frames, 1);
        }

        float* data = buf.getBuffer().data();
        double c = ctx.cycle;
        for (int i = 0; i < ctx.frames; i++) {
            data[i] = pattern->eval(c);
            c += ctx.cycleStep;
        }
    }
}

void Node::pullAudio(ofSoundBuffer& buffer, int index) {
    if (!active->get()) {
        processAudioBypass(buffer, index);
        return;
    }
    processAudio(buffer, index);
}

void Node::processAudioBypass(ofSoundBuffer& buffer, int index) {
    buffer.set(0);
}

ofTexture* Node::getVideoOutput(int index) {
    if (!active->get()) return processVideoBypass(index);
    return processVideo(index);
}

ofTexture* Node::processVideoBypass(int index) {
    return nullptr;
}

Control Node::getControl(ofParameter<float>& param) const {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    auto it = controlBuffers.find(param.getName());
    if (it != controlBuffers.end()) {
        return { it->second.getBuffer().data(), 0.0f, true };
    }
    return { nullptr, param.get(), false };
}

void Node::setInputNode(int slot, Node* node) {
    inputNodes[slot] = node;
}

Node* Node::getInputNode(int slot) const {
    auto it = inputNodes.find(slot);
    if (it != inputNodes.end()) return it->second;
    return nullptr;
}

void Node::modulate(const std::string& paramName, std::shared_ptr<Pattern<float>> pat) {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    ofLogNotice("Node") << "modulate: " << name << " param=" << paramName
                        << " pattern=" << (pat ? pat->getSignature() : "null");
    modulators[paramName] = pat;
}

void Node::clearModulator(const std::string& paramName) {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    modulators.erase(paramName);

    // Send a null pattern to clear the slot on the audio/video thread
    if (audioProcessor || videoProcessor) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::SET_PATTERN;
        cmd.slotName = paramName;
        cmd.pattern = nullptr;
        pushCommand(cmd);
    }
}

std::shared_ptr<Pattern<float>> Node::getPattern(const std::string& paramName) const {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    auto it = modulators.find(paramName);
    if (it != modulators.end()) return it->second;
    return nullptr;
}

void Node::onParameterChanged(const std::string& paramName) {
    if (!audioProcessor && !videoProcessor) {
        ofLogWarning("Node") << "onParameterChanged: no processor for node "
                             << name << " param " << paramName;
        return;
    }

    float val = 0;
    bool found = false;

    for (int i = 0; i < (int)parameters->size(); i++) {
        if (parameters->getName(i) == paramName) {
            auto& p = parameters->get(i);
            if (p.type() == typeid(ofParameter<float>).name()) {
                val = p.cast<float>().get();
                found = true;
            } else if (p.type() == typeid(ofParameter<bool>).name()) {
                val = p.cast<bool>().get() ? 1.0f : 0.0f;
                found = true;
            } else if (p.type() == typeid(ofParameter<int>).name()) {
                val = (float)p.cast<int>().get();
                found = true;
            }
            break;
        }
    }

    if (found) {
        ofLogNotice("Node") << "onParameterChanged: " << name
                            << " param=" << paramName << " value=" << val;

        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::SET_PARAM;
        cmd.slotName = paramName;
        cmd.value = val;
        pushCommand(cmd);

        // Also propagate any pattern for this parameter to the audio/video thread.
        // This is the single point where patterns are sent to processors.
        {
            std::lock_guard<std::recursive_mutex> lock(modMutex);
            auto it = modulators.find(paramName);
            if (it != modulators.end()) {
                ofLogNotice("Node") << "onParameterChanged: " << name
                                    << " sending pattern for " << paramName;
                crumble::ProcessorCommand patCmd;
                patCmd.type = crumble::ProcessorCommand::SET_PATTERN;
                patCmd.slotName = paramName;
                patCmd.pattern = it->second;
                pushCommand(patCmd);
            }
        }
    }
}

std::string Node::resolvePath(const std::string& path, const std::string& typeHint) const {
    if (graph) return graph->resolvePath(path, typeHint);
    return path;
}

ofJson Node::serialize() const {
    ofJson j;
    ofSerialize(j, *parameters);
    return j;
}

void Node::deserialize(const ofJson& json) {
    ofDeserialize(json, *parameters);
}