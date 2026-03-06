#include "Node.h"
#include "Graph.h"
#include "Transport.h"

// Static counter for generating unique nodeIds
std::atomic<int> Node::nextNodeId(0);

Node::Node() {
    parameters.setName("group");
    parameters.add(volume.set("volume", 1.0, 0.0, 1.0));
    parameters.add(opacity.set("opacity", 1.0, 0.0, 1.0));
    parameters.add(active.set("active", true));
    drawLayer.set("drawLayer", 0, -100, 100);
}

void Node::pullAudio(ofSoundBuffer& buffer, int index) {
    if (active) {
        processAudio(buffer, index);
    } else {
        processAudioBypass(buffer, index);
    }
}

ofTexture* Node::getVideoOutput(int index) {
    if (active) {
        return processVideo(index);
    } else {
        return processVideoBypass(index);
    }
}

void Node::processAudioBypass(ofSoundBuffer& buffer, int index) {
    buffer.set(0); // Identity: Silence
}

ofTexture* Node::processVideoBypass(int index) {
    return nullptr; // Identity: Transparency
}

void Node::prepare(const Context& ctx) {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    
    // Vectorized pre-calculation for audio/control blocks
    if (ctx.frames <= 1) return;
    
    for (auto& [paramName, pattern] : modulators) {
        if (!pattern) continue;

        ofSoundBuffer& buffer = controlBuffers[paramName];
        if ((int)buffer.getNumFrames() != ctx.frames) {
            buffer.allocate(ctx.frames, 1);
        }

        // Sample pattern at hardware-aligned steps for graph-wide sync
        float* samples = buffer.getBuffer().data();
        for (int i = 0; i < ctx.frames; i++) {
            double currentCycle = ctx.cycle + (i * ctx.cycleStep);
            samples[i] = pattern->eval(currentCycle);
        }
    }
}

Control Node::getControl(ofParameter<float>& param) const {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    
    Control ctrl;
    ctrl.constant = param.get();
    
    auto modIt = modulators.find(param.getName());
    if (modIt != modulators.end() && modIt->second) {
        ctrl.modulated = true;
        
        // If there's a pre-calculated buffer (from prepare), use it
        auto bufIt = controlBuffers.find(param.getName());
        if (bufIt != controlBuffers.end() && bufIt->second.getNumFrames() > 1) {
            ctrl.buffer = bufIt->second.getBuffer().data();
        } else {
            // No buffer (e.g. video update), evaluate the pattern right now
            if (graph) {
                ctrl.constant = modIt->second->eval(graph->getTransport().cycle);
            }
            ctrl.modulated = false; // Baked into constant for this single-sample read
        }
    } else {
        ctrl.modulated = false;
        ctrl.constant = param.get(); 
    }
    
    return ctrl;
}

ofJson Node::serialize() const {
    ofJson j;
    ofSerialize(j, parameters);
    return j;
}

void Node::deserialize(const ofJson& json) {
    if (!json.is_object()) return;
    ofDeserialize(json, parameters);
}

std::string Node::resolvePath(const std::string& path, const std::string& typeHint) const {
    if (graph) {
        return graph->resolvePath(path, typeHint);
    }
    return ofToDataPath(path);
}
