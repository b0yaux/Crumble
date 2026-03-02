#include "Node.h"
#include "Graph.h"
#include "Transport.h"

// Static counter for generating unique nodeIds
std::atomic<int> Node::nextNodeId(0);

Node::Node() {
    drawLayer.set("drawLayer", 0, -100, 100);
}

void Node::prepare(const Context& ctx) {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    
    // Only pre-calculate buffers if this is an audio/control block (frames > 1)
    // Video updates (frames == 1) evaluate on the fly in getControl()
    if (ctx.frames <= 1) return;
    
    // Iterate through all parameters that have patterns (modulators) assigned
    for (auto& [paramName, pattern] : modulators) {
        if (!pattern) continue;

        // Ensure the control buffer is correctly sized
        ofSoundBuffer& buffer = controlBuffers[paramName];
        if ((int)buffer.getNumFrames() != ctx.frames) {
            buffer.allocate(ctx.frames, 1);
        }

        // Fill the buffer with values from the pattern
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
