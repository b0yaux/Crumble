#include "Session.h"
#include "../nodes/SpeakersOutput.h"

Session* g_session = nullptr;

Session::Session() {
    g_session = this;
    setupAudio(44100, 256); // Start silent audio engine immediately
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
    // 1. Advance the absolute clock (Hardware precision)
    float dt = buffer.getNumFrames() / (float)buffer.getSampleRate();
    transport.update(dt);
    
    // 2. Clear buffer to silence initially
    buffer.set(0);
    
    // 3. Instead of pulling from SpeakersOutput directly, we ask the Graph.
    // The Graph handles pushing the Context to all nodes before pulling.
    
    // However, Graph::pullAudio looks for an "Outlet". But in the root session, 
    // the user connects to a "SpeakersOutput". We need the Graph to know how to 
    // push Context BEFORE pulling from the SpeakersOutput.
    
    std::lock_guard<std::recursive_mutex> lock(graph.getAudioMutex());
    
    // 3. Push timing to all nodes recursively (The "Push" phase)
    Context ctx;
    ctx.cycle = transport.cycle;
    ctx.cycleStep = transport.getCyclesPerSample(buffer.getSampleRate());
    ctx.frames = buffer.getNumFrames();
    
    graph.prepare(ctx);
    
    // 4. Find the hardware sink (SpeakersOutput) and pull audio
    for (const auto& [id, node] : graph.getNodes()) {
        if (node->type == "SpeakersOutput") {
            node->pullAudio(buffer, 0);
            break; // Just use the first one
        }
    }
}

// --- Graph primitives ---

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

// --- Script lifecycle ---

void Session::beginScript() {
    for (auto& [nodeId, node] : graph.getNodes()) {
        node->touched = false;
    }
}

void Session::endScript() {
    // Remove untouched nodes - iterate over a copy of node IDs since we modify the map
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

// --- Lifecycle ---

void Session::update(float dt) {
    // 1. Prepare all nodes recursively for UI/Video (Main thread pass)
    // The 'lastPreparedCycle' optimization in Node::prepare ensures 
    // we don't redundant work if the audio thread already ran.
    Context ctx;
    ctx.cycle = transport.cycle;
    ctx.cycleStep = 0; // No sub-sample stepping for UI
    ctx.frames = 1;
    ctx.dt = dt;
    
    {
        std::lock_guard<std::recursive_mutex> lock(graph.getAudioMutex());
        graph.prepare(ctx);
    }

    graph.update(dt);
}

void Session::draw() {
    graph.draw();
}

// --- Node access ---

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

// --- Persistence ---

bool Session::save(const std::string& path) {
    return graph.saveToFile(path);
}

bool Session::load(const std::string& path) {
    return graph.loadFromFile(path);
}

// --- Factory ---

void Session::registerNodeType(const std::string& type, Graph::NodeCreator creator) {
    graph.registerNodeType(type, creator);
}

std::vector<std::string> Session::getRegisteredTypes() const {
    return graph.getRegisteredTypes();
}

// --- Direct graph access ---

Graph& Session::getGraph() { return graph; }
const Graph& Session::getGraph() const { return graph; }
