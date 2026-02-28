#include "Transport.h"

void Transport::update(float dt) {
    if (!isPlaying) return;
    
    absoluteTime += dt;
    
    // Calculate cycles based on BPM
    // 1 beat = 60 / BPM seconds
    // Cycles per second = BPM / 60
    double cyclesPerSecond = bpm / 60.0;
    
    // Increment cycle
    cycle += dt * cyclesPerSecond;
    
    // Wrap cycle to [0.0, 1.0)
    // using while loop handles large dt or fast BPMs better than simple fmod sometimes
    while (cycle >= 1.0) {
        cycle -= 1.0;
    }
}
