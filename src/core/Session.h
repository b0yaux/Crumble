#pragma once
#include "ofMain.h"
#include "Graph.h"
#include "AssetCache.h"
#include "Transport.h"
#include "ProcessorCommand.h"
#include "moodycamel/readerwriterqueue.h"
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

namespace crumble {
    // Wrap moodycamel for easier use in Crumble
    template<typename T>
    using SPSCQueue = moodycamel::ReaderWriterQueue<T>;
}

// Global session pointer for nodes that need asset access
extern class Session* g_session;

// Session — the live working context for a Crumble session.
class Session {
public:
    Session();
    ~Session();

    // --- Asset Management ---
    AssetCache& getCache() { return assetCache; }

    // --- Graph primitives ---
    Node* addNode(const std::string& type, const std::string& name = "");
    void  removeNode(int nodeId);
    void  connect(int fromNode, int toNode, int fromOutput = 0, int toInput = 0);
    void  disconnect(int toNode, int toInput = 0);
    void  clear();
    
    // --- Script lifecycle ---
    void beginScript();
    void endScript();
    void touchNode(int nodeId);

    // --- Lifecycle ---
    void setupAudio(int sampleRate = 44100, int bufferSize = 256);
    void update(float dt);
    void draw();
    void audioOut(ofSoundBuffer& buffer);

    // --- Node access ---
    Node* getNode(int nodeId);
    int   getNodeCount() const;
    Node* findNodeByName(const std::string& name);
    
    // --- Persistence ---
    bool save(const std::string& path);
    bool load(const std::string& path);

    // --- Factory ---
    void registerNodeType(const std::string& type, Graph::NodeCreator creator);
    std::vector<std::string> getRegisteredTypes() const;

    Graph&       getGraph() { return graph; }
    const Graph& getGraph() const { return graph; }

    // --- Transport ---
    Transport& getTransport() { return transport; }
    const Transport& getTransport() const { return transport; }

    // --- Wait-Free Messaging ---
    void sendCommand(const crumble::ProcessorCommand& cmd);

    // --- Audio Endpoint Registration ---
    // Called by nodes that are session-driven audio endpoints (e.g. SpeakersOutput).
    // Enqueues a REGISTER_ENDPOINT command so the audio thread maintains the list.
    void registerAudioEndpoint(crumble::AudioProcessor* ap);

private:
    Graph graph;
    AssetCache assetCache;
    Transport transport;
    uint64_t frameCounter = 0;
    ofSoundStream soundStream;

    // The "Air-Gap" Queues (Audio Thread)
    crumble::SPSCQueue<crumble::ProcessorCommand> audioCommandQueue{1024};
    crumble::SPSCQueue<crumble::AudioProcessor*> audioReleaseQueue{1024};
    std::vector<crumble::AudioProcessor*> activeAudioProcessors;

    // Processors nominated as session-driven audio endpoints.
    // Populated and iterated exclusively on the audio thread via REGISTER_ENDPOINT
    // and REMOVE_NODE commands — no locks required.
    std::vector<crumble::AudioProcessor*> audioEndpoints;

    // The "Air-Gap" Queues (Video - Evaluated on Main Thread)
    crumble::SPSCQueue<crumble::ProcessorCommand> videoCommandQueue{1024};
    crumble::SPSCQueue<crumble::VideoProcessor*> videoReleaseQueue{1024};
    std::vector<crumble::VideoProcessor*> activeVideoProcessors;
    

};
