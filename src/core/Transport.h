#pragma once
#include <cmath>

class Transport {
public:
    Transport() : bpm(120.0f), absoluteTime(0.0), cycle(0.0), isPlaying(true) {}
    
    // Core timing update (usually called from audio thread)
    void update(float dt);
    
    // Core timing state
    float bpm;              // Beats per minute
    double absoluteTime;    // Total running time in seconds
    double cycle;           // Current beat phase (0.0 to 1.0)
    
    bool isPlaying;

    // Sub-sample accuracy for modulators/parameters
    double getCyclesPerSample(int sampleRate) const {
        double cyclesPerSecond = bpm / 60.0;
        return cyclesPerSecond / (double)sampleRate;
    }

    double getCycleAtSample(int sampleIndex, int sampleRate) const {
        double exactCycle = cycle + (sampleIndex * getCyclesPerSample(sampleRate));
        // Wrap to [0, 1) using fmod or manual subtraction for performance
        double c = std::fmod(exactCycle, 1.0);
        if (c < 0) c += 1.0;
        return c;
    }
};
