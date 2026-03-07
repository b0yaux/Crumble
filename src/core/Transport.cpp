#include "Transport.h"

void Transport::update(float dt) {
    if (!isPlaying) return;

    absoluteTime += dt;

    // Advance cycle in bars (4/4 assumed).
    // bpm / 60.0  = beats per second
    // ÷ 4         = bars per second  (1 bar = 4 beats in 4/4)
    // So at 120 BPM: 120/60/4 = 0.5 bars/s → cycle wraps every 2 seconds.
    // This matches the documented contract: osc(1.0) = one cycle per bar.
    double barsPerSecond = (bpm / 60.0) / 4.0;
    cycle += dt * barsPerSecond;

    while (cycle >= 1.0) cycle -= 1.0;
}
