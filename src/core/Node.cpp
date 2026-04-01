#include "ofMain.h"
#include "Node.h"
#include "Graph.h"
#include "Session.h"
#include "AudioCache.h"
#include "ofxAudioFile.h"
#include "NodeProcessor.h"
#include "ProcessorCommand.h"

std::atomic<int> Node::nextNodeId{0};

Node::Node() {
    parameters = std::make_shared<ofParameterGroup>();
    parameters->setName("parameters");
    
    gain = std::make_shared<ofParameter<float>>();
    parameters->add(gain->set("gain", 1.0, 0.0, 4.0));
    
    opacity = std::make_shared<ofParameter<float>>();
    parameters->add(opacity->set("opacity", 1.0, 0.0, 1.0));
    
    blend = std::make_shared<ofParameter<int>>();
    parameters->add(blend->set("blend", 0, 0, 2)); // Default: ALPHA
    
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

    // Synchronize shadow processor control slots with current parameter values.
    // This ensures newly created nodes or those re-created during reload start with 
    // the correct state in the processing threads.
    for (int i = 0; i < (int)parameters->size(); i++) {

        auto& param = parameters->get(i);
        float val = 0;
        bool supported = false;

        std::string vt = param.valueType();
        // Robust type mapping: handles compiler-specific mangled names and short codes
        // to ensure cross-platform parameter synchronization.
        if (vt == "f" || vt == "float" || vt == "d" || vt == "double" || vt == typeid(float).name() || vt == typeid(double).name()) {

            val = (float)param.cast<float>().get();
            supported = true;
        } else if (vt == "b" || vt == "bool" || vt == typeid(bool).name()) {
            val = param.cast<bool>().get() ? 1.0f : 0.0f;
            supported = true;
        } else if (vt == "i" || vt == "int" || vt == typeid(int).name()) {
            val = (float)param.cast<int>().get();
            supported = true;
        }

        if (supported) {
            uint32_t hash = crumble::hashString(param.getName().c_str());
            if (audioProcessor) {
                audioProcessor->nodeId = nodeId;
                if (auto* slot = audioProcessor->getControlPtr(hash)) {
                    slot->value.store(val, std::memory_order_relaxed);
                }
            }
            if (videoProcessor) {
                videoProcessor->nodeId = nodeId;
                if (auto* slot = videoProcessor->getControlPtr(hash)) {
                    slot->value.store(val, std::memory_order_relaxed);
                }
            }
        }
    }

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
        if (cmd.nodeId == -1) cmd.nodeId = nodeId;
        if (!cmd.audioProcessor) cmd.audioProcessor = audioProcessor;
        if (!cmd.videoProcessor) cmd.videoProcessor = videoProcessor;
        g_session->sendCommand(cmd);
    }
}

void Node::prepare(const Context& ctx) {
    if (lastCtx.cycle == ctx.cycle && ctx.frames > 1) return;
    lastCtx = ctx;

    std::lock_guard<std::recursive_mutex> lock(modMutex);

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
    if (node) inputNodes[slot] = node;
    else inputNodes.erase(slot);
}

Node* Node::getInputNode(int slot) const {
    auto it = inputNodes.find(slot);
    if (it != inputNodes.end()) return it->second;
    return nullptr;
}

void Node::modulate(const std::string& paramName, std::shared_ptr<Pattern<float>> pat) {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    modulators[paramName] = pat;
    modulatorsTouched.insert(paramName);
}

void Node::clearModulator(const std::string& paramName) {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    modulators.erase(paramName);
    modulatorsTouched.insert(paramName);

    if (audioProcessor || videoProcessor) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::SET_PATTERN;
        cmd.paramHash = crumble::hashString(paramName.c_str());
        cmd.pattern = nullptr;
        pushCommand(cmd);
    }
    
    // Explicitly sync the parameter value after clearing modulator to ensure fallback is correct
    onParameterChanged(paramName);
}

void Node::clearModulators() {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    std::vector<std::string> paramNames;
    for (const auto& [name, _] : modulators) paramNames.push_back(name);
    for (const auto& name : paramNames) clearModulator(name);
}

void Node::clearUntouchedModulators() {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    std::vector<std::string> toRemove;
    for (const auto& [name, _] : modulators) {
        if (modulatorsTouched.find(name) == modulatorsTouched.end()) {
            toRemove.push_back(name);
        }
    }
    modulatorsTouched.clear();
    for (const auto& name : toRemove) clearModulator(name);
}

std::shared_ptr<Pattern<float>> Node::getPattern(const std::string& paramName) const {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    auto it = modulators.find(paramName);
    if (it != modulators.end()) return it->second;
    return nullptr;
}

void Node::onParameterChanged(const std::string& paramName) {
    if (!audioProcessor && !videoProcessor) return;

    float val = 0;
    bool found = false;

    if (paramName == "active") {
        val = active->get() ? 1.0f : 0.0f;
        found = true;
    } else if (paramName == "gain") {
        val = gain->get();
        found = true;
    } else if (paramName == "opacity") {
        val = opacity->get();
        found = true;
    } else if (paramName == "blend") {
        val = (float)blend->get();
        found = true;
    } else {
        for (int i = 0; i < (int)parameters->size(); i++) {
            if (parameters->getName(i) == paramName) {
                auto& p = parameters->get(i);
                std::string vt = p.valueType();
                if (vt == "f" || vt == "float" || vt == "d" || vt == "double" || vt == typeid(float).name() || vt == typeid(double).name()) {
                    val = p.cast<float>().get();
                    found = true;
                } else if (vt == "b" || vt == "bool" || vt == typeid(bool).name()) {
                    val = p.cast<bool>().get() ? 1.0f : 0.0f;
                    found = true;
                } else if (vt == "i" || vt == "int" || vt == typeid(int).name()) {
                    val = (float)p.cast<int>().get();
                    found = true;
                }
                break;
            }
        }
    }

    if (found) {
        crumble::ProcessorCommand cmd;
        cmd.type = crumble::ProcessorCommand::SET_PARAM;
        cmd.nodeId = nodeId;
        cmd.paramHash = crumble::hashString(paramName.c_str());
        cmd.value = val;
        pushCommand(cmd);
    }

    {
        std::lock_guard<std::recursive_mutex> lock(modMutex);
        auto it = modulators.find(paramName);
        if (it != modulators.end()) {
            crumble::ProcessorCommand patCmd;
            patCmd.type = crumble::ProcessorCommand::SET_PATTERN;
            patCmd.paramHash = crumble::hashString(paramName.c_str());
            patCmd.pattern = it->second;
            pushCommand(patCmd);
        }
    }
}

std::string Node::resolvePath(const std::string& path, const std::string& typeHint) const {
    if (graph) return graph->resolvePath(path, typeHint);
    return path;
}

std::string Node::extractBank(const std::string& path) {
    if (path.empty() || path.find('/') != std::string::npos) return "";
    size_t colon = path.find(':');
    return (colon != std::string::npos) ? path.substr(0, colon) : path;
}

AudioCache* Node::getCache() const {
    if (graph) return graph->getCache();
    return nullptr;
}

int Node::getSampleRate() const {
    if (graph) return graph->getSampleRate();
    return 44100;
}

std::shared_ptr<ofxAudioFile> Node::getAudioAsset(const std::string& path) const {
    auto cache = getCache();
    if (cache) return cache->getAudio(path);
    return nullptr;
}

ofJson Node::serialize() const {
    ofJson j;
    ofSerialize(j, *parameters);
    return j;
}

void Node::deserialize(const ofJson& json) {
    ofDeserialize(json, *parameters);
}
