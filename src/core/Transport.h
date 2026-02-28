#pragma once

class Transport {
public:
    Transport() : bpm(120.0f), absoluteTime(0.0), cycle(0.0), isPlaying(true) {}
    
    void update(float dt);
    
    // Core timing state
    float bpm;              // Beats per minute
    double absoluteTime;    // Total running time in seconds
    double cycle;           // Current beat phase (e.g. 0.0 to 1.0)
    
    // Derived/helper variables might be added later (e.g., bars, playback state)
    bool isPlaying;
};
