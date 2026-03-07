#include "Transport.h"

void Transport::update(float dt) {
    if (!isPlaying) return;

    absoluteTime += dt;

    // Advance cycle in bars.
    // bpm / 60.0        = beats per second
    // ÷ beatsPerBar     = bars per second
    //
    // Examples at 120 BPM:
    //   4/4 (default): 120/60/4 = 0.5 bars/s  → cycle wraps every 2.0 s
    //   3/4:           120/60/3 = 0.667 bars/s → cycle wraps every 1.5 s
    //   5/4:           120/60/5 = 0.4 bars/s   → cycle wraps every 2.5 s
    //
    // Pattern contract: osc(1.0) completes exactly one oscillation per bar,
    // regardless of time signature or BPM.
    double barsPerSecond = (bpm / 60.0) / beatsPerBar;
    cycle += dt * barsPerSecond;

    while (cycle >= 1.0) cycle -= 1.0;
}