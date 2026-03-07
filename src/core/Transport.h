#pragma once
#include <cmath>

class Transport {
public:
    Transport() : bpm(120.0f), absoluteTime(0.0), cycle(0.0), isPlaying(true) {}

    // Core timing update (called from audio thread each block)
    void update(float dt);

    // Core timing state
    float bpm;              // Beats per minute
    double absoluteTime;    // Total running time in seconds
    double cycle;           // Current bar phase [0.0, 1.0) — wraps once per 4-beat bar (4/4)

    bool isPlaying;

    // Returns bars per sample — use as cycleStep in sample-accurate pattern evaluation.
    // At 120 BPM: (120/60)/4 / sampleRate = 0.5 / 44100 ≈ 1.13e-5 bars/sample.
    double getCyclesPerSample(int sampleRate) const {
        double barsPerSecond = (bpm / 60.0) / 4.0;
        return barsPerSecond / (double)sampleRate;
    }

    // Returns the bar phase at a specific sample offset within the current block.
    double getCycleAtSample(int sampleIndex, int sampleRate) const {
        double exactCycle = cycle + (sampleIndex * getCyclesPerSample(sampleRate));
        double c = std::fmod(exactCycle, 1.0);
        if (c < 0) c += 1.0;
        return c;
    }
};
