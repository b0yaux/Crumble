#include "ofMain.h"
#include "Node.h"
#include "Graph.h"
#include "Session.h"
#include "NodeProcessor.h"
#include "AudioCommand.h"

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
    if (processor && g_session) {
        crumble::AudioCommand cmd;
        cmd.type = crumble::AudioCommand::REMOVE_NODE;
        cmd.processor = processor;
        g_session->sendCommand(cmd);
    }
}

void Node::setupProcessor() {
    if (processor) return;
    processor = createProcessor();
    if (processor) {
        processor->nodeId = nodeId;
        
        // GENERIC INITIAL SYNC: Loop through all parameters in the group.
        // Also populate the slotMap so processors can look up slots by name.
        for (int i = 0; i < (int)parameters->size(); i++) {
            auto& p = parameters->get(i);
            float val = 0;
            bool supported = false;
            
            if (p.type() == typeid(ofParameter<float>).name()) {
                val = p.cast<float>().get();
                supported = true;
            } else if (p.type() == typeid(ofParameter<bool>).name()) {
                val = p.cast<bool>().get() ? 1.0f : 0.0f;
                supported = true;
            } else if (p.type() == typeid(ofParameter<int>).name()) {
                val = (float)p.cast<int>().get();
                supported = true;
            }
            
            if (supported) {
                processor->values[i].store(val);
                // Register in slotMap so processors can use getSlot("name")
                processor->slotMap[p.getName()] = i;
            }
        }
        
        crumble::AudioCommand cmd;
        cmd.type = crumble::AudioCommand::ADD_NODE;
        cmd.processor = processor;
        pushCommand(cmd);
    }
}

void Node::pushCommand(crumble::AudioCommand cmd) {
    if (g_session) {
        cmd.nodeId = nodeId;
        if (!cmd.processor) cmd.processor = processor;
        g_session->sendCommand(cmd);
    }
}

void Node::registerSlot(const std::string& paramName, int slotIndex) {
    if (!processor) return;
    // Update the processor's slotMap directly (safe from UI thread before audio starts)
    // and also send a command so the audio thread picks it up at runtime.
    processor->slotMap[paramName] = slotIndex;

    crumble::AudioCommand cmd;
    cmd.type = crumble::AudioCommand::SET_SLOT;
    cmd.slotName = paramName;
    cmd.slotIndex = slotIndex;
    pushCommand(cmd);
}

void Node::prepare(const Context& ctx) {
    if (lastPreparedCycle == ctx.cycle && ctx.frames > 1) return;
    lastPreparedCycle = ctx.cycle;

    std::lock_guard<std::recursive_mutex> lock(modMutex);
    
    // Update controlBuffers for UI/video use (getControl()).
    // We do NOT send SET_PATTERN here — patterns are sent once in modulate()
    // and evaluated sample-accurately by the audio thread via patternSlots.
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
    modulators[paramName] = pat;

    // Send the pattern to the audio thread immediately so it can evaluate
    // it inline per sample. No buffer allocation needed — patterns are stateless.
    if (processor) {
        int slot = -1;
        for (int i = 0; i < (int)parameters->size(); i++) {
            if (parameters->getName(i) == paramName) { slot = i; break; }
        }
        if (slot >= 0) {
            crumble::AudioCommand cmd;
            cmd.type = crumble::AudioCommand::SET_PATTERN;
            cmd.slotIndex = slot;
            cmd.pattern = pat; // shared_ptr copy — ref-count is atomic, safe across threads
            pushCommand(cmd);
        }
    }
}

void Node::clearModulator(const std::string& paramName) {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    modulators.erase(paramName);

    // Send a null pattern to clear the slot on the audio thread
    if (processor) {
        int slot = -1;
        for (int i = 0; i < (int)parameters->size(); i++) {
            if (parameters->getName(i) == paramName) { slot = i; break; }
        }
        if (slot >= 0) {
            crumble::AudioCommand cmd;
            cmd.type = crumble::AudioCommand::SET_PATTERN;
            cmd.slotIndex = slot;
            cmd.pattern = nullptr; // clears patternSlots[slot] on audio thread
            pushCommand(cmd);
        }
    }
}

std::shared_ptr<Pattern<float>> Node::getPattern(const std::string& paramName) const {
    auto it = modulators.find(paramName);
    if (it != modulators.end()) return it->second;
    return nullptr;
}

void Node::onParameterChanged(const std::string& paramName) {
    if (!processor) return;
    
    int slot = -1;
    float val = 0;
    
    for (int i = 0; i < (int)parameters->size(); i++) {
        if (parameters->getName(i) == paramName) {
            auto& p = parameters->get(i);
            if (p.type() == typeid(ofParameter<float>).name()) {
                slot = i;
                val = p.cast<float>().get();
            } else if (p.type() == typeid(ofParameter<bool>).name()) {
                slot = i;
                val = p.cast<bool>().get() ? 1.0f : 0.0f;
            } else if (p.type() == typeid(ofParameter<int>).name()) {
                slot = i;
                val = (float)p.cast<int>().get();
            }
            break;
        }
    }
    
    if (slot >= 0) {
        crumble::AudioCommand cmd;
        cmd.type = crumble::AudioCommand::SET_PARAM;
        cmd.slotIndex = slot;
        cmd.value = val;
        pushCommand(cmd);
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
