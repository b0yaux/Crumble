#pragma once
#include "ofMain.h"
#include "Graph.h"
#include "AudioCache.h"
#include "Transport.h"
#include "InputBindings.h"
#include "ProcessorCommand.h"
#include "moodycamel/readerwriterqueue.h"
#include <vector>
#include <unordered_set>
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
    AudioCache& getCache() { return audioCache; }

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

    int getSampleRate() const { return soundStream.getSampleRate(); }

    // --- Transport ---
    Transport& getTransport() { return transport; }
    const Transport& getTransport() const { return transport; }

    // --- Input Bindings ---
    crumble::InputBindings& getInputBindings() { return inputBindings; }

    // --- Wait-Free Messaging ---
    void sendCommand(const crumble::ProcessorCommand& cmd);

    // --- Audio Endpoint Registration ---
    // Called by nodes that are session-driven audio endpoints (e.g. SpeakersOutput).
    // Enqueues a REGISTER_ENDPOINT command so the audio thread maintains the list.
    void registerAudioEndpoint(crumble::AudioProcessor* ap);

private:
    static constexpr int COMMAND_QUEUE_CAPACITY = 1024;

    Graph graph;
    AudioCache audioCache;
    Transport transport;
    crumble::InputBindings inputBindings;
    uint64_t frameCounter = 0;
    ofSoundStream soundStream;

    // The "Air-Gap" Queues (Audio Thread)
    crumble::SPSCQueue<crumble::ProcessorCommand> audioCommandQueue{COMMAND_QUEUE_CAPACITY};
    crumble::SPSCQueue<crumble::AudioProcessor*> audioReleaseQueue{COMMAND_QUEUE_CAPACITY};
    // Commands containing displaced patterns from the audio thread are enqueued here
    // so their destructors run on the main thread, not inside the real-time callback.
    crumble::SPSCQueue<crumble::ProcessorCommand> commandGarbageQueue{COMMAND_QUEUE_CAPACITY};
    // unordered_set makes the alive() O(1) check inside audioOut() constant-time
    // regardless of how many processors are registered.
    std::unordered_set<crumble::AudioProcessor*> activeAudioProcessors;

    // Processors nominated as session-driven audio endpoints.
    // Populated and iterated exclusively on the audio thread via REGISTER_ENDPOINT
    // and REMOVE_NODE commands — no locks required.
    std::vector<crumble::AudioProcessor*> audioEndpoints;

    // The "Air-Gap" Queues (Video - Evaluated on Main Thread)
    crumble::SPSCQueue<crumble::ProcessorCommand> videoCommandQueue{COMMAND_QUEUE_CAPACITY};
    crumble::SPSCQueue<crumble::VideoProcessor*> videoReleaseQueue{COMMAND_QUEUE_CAPACITY};
    // unordered_set for O(1) alive() check in Session::update()
    std::unordered_set<crumble::VideoProcessor*> activeVideoProcessors;
    

};
