#pragma once
#include <atomic>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <array>
#include <vector>
#include "moodycamel/readerwriterqueue.h"
#include "ofxMidi.h"

namespace crumble {

class InputBindingsImpl;

/**
 * InputBindings: Maps external hardware inputs (MIDI, OSC, Gamepad) to atomic floats.
 * 
 * Design:
 * 1. Pointer Stability: Atomic floats never move in memory once created.
 * 2. Pre-allocated MIDI: Fast-path for MIDI (CC, Notes, Touch).
 */
class InputBindings {
public:
    InputBindings();
    ~InputBindings();
    
    void setup();
    void update();
    void cleanup();

    // Get or create an atomic binding for a semantic path
    std::atomic<float>* getBinding(const std::string& path);
    
    // Fast path for MIDI bindings
    std::atomic<float>* getMidiBinding(int statusOffset, int chan, int num);

    // Update a binding value from the main thread
    void setBinding(const std::string& path, float value);

    // --- MIDI Note Events (discrete, ring buffer) ---

    /// Push a MIDI note event to the ring buffer. Thread-safe (called from MIDI callback).
    void pushMidiNoteEvent(const ofxMidiMessage& event);

    /// Drain all pending MIDI note events for a channel (0 = all channels).
    /// Called from the main thread. Returns events in arrival order.
    std::vector<ofxMidiMessage> drainMidiNoteEvents(int channel = 0);

private:
    // MIDI Storage: [StatusOffset][Channel][Number]
    // 0:CC, 1:Note, 2:Touch
    // 3 status * 16 channels * 128 numbers = 6144 slots
    std::array<std::atomic<float>, 6144> midiStore;

    // Named Storage (OSC/Gamepad) - using unique_ptr to ensure pointer stability
    std::unordered_map<std::string, std::unique_ptr<std::atomic<float>>> namedStore;
    
    std::mutex mutex;
    std::unique_ptr<InputBindingsImpl> impl;

    // Internal helper to get MIDI index
    int getMidiIndex(int statusOffset, int chan, int num);

    // Ring buffer for MIDI note events. SPSC: MIDI callback writes, main thread reads.
    // Capacity 256 covers rapid playing (e.g. 16th notes at 200 BPM = ~53 events/second).
    static constexpr int MIDI_EVENT_QUEUE_SIZE = 256;
    using MidiEventQueue = moodycamel::ReaderWriterQueue<ofxMidiMessage>;
    MidiEventQueue midiNoteQueue{MIDI_EVENT_QUEUE_SIZE};
};

} // namespace crumble
