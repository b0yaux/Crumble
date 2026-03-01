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
    
    // Only pre-calculate buffers if this is an audio block (frames > 1)
    // Video updates (frames == 1) will just evaluate on the fly using getSignal()[0]
    if (ctx.frames <= 1) return;
    
    // Iterate through all parameters that have modulators
    for (auto& [paramName, generator] : modulators) {
        if (!generator) continue;

        // Ensure the signal buffer is correctly sized
        ofSoundBuffer& buffer = signalBuffers[paramName];
        if ((int)buffer.getNumFrames() != ctx.frames) {
            buffer.allocate(ctx.frames, 1);
        }

        // Fill the buffer with values from the generator
        float* samples = buffer.getBuffer().data();
        for (int i = 0; i < ctx.frames; i++) {
            double currentCycle = ctx.cycle + (i * ctx.cycleStep);
            samples[i] = generator->eval(currentCycle);
        }
    }
}

Signal Node::getSignal(ofParameter<float>& param) const {
    std::lock_guard<std::recursive_mutex> lock(modMutex);
    
    Signal sig;
    sig.constant = param.get();
    
    auto modIt = modulators.find(param.getName());
    if (modIt != modulators.end() && modIt->second) {
        sig.modulated = true;
        
        // If there's a pre-calculated buffer (from audio prepare), use it
        auto bufIt = signalBuffers.find(param.getName());
        if (bufIt != signalBuffers.end() && bufIt->second.getNumFrames() > 1) {
            sig.buffer = bufIt->second.getBuffer().data();
        } else {
            // No buffer (e.g. video update), just evaluate the constant right now
            if (graph) {
                sig.constant = modIt->second->eval(graph->getTransport().cycle);
            }
            sig.modulated = false; // We baked the modulation into the constant for this single read
        }
    } else {
        sig.modulated = false;
        sig.constant = param.get(); // Ensure fallback is the actual ofParameter value!
    }
    
    return sig;
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
