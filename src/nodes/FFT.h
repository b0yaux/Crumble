#pragma once
#include "../core/Node.h"

/**
 * FFT: Audio passthrough that performs spectral analysis.
 *
 * Sits in the audio chain — pulls input, copies to output unchanged, and runs
 * FFT analysis as a side effect. Results are exposed as atomic floats that
 * External patterns can read for modulation (e.g. f:bass():scale(0, 2)).
 *
 * Parameters:
 *   size      — FFT size in samples (512, 1024, 2048, 4096). Default 2048.
 *   smoothing — Spectral decay 0–1. Higher = smoother/slower. Default 0.8.
 */
class FFT : public Node {
public:
    FFT();

    ofParameter<float> size;
    ofParameter<float> smoothing;

    crumble::AudioProcessor* createAudioProcessor() override;
    std::string getDisplayName() const override { return "FFT"; }

    /// Called each frame — refreshes band atomics from processor bin atomics.
    void update(float dt) override;

    /// Number of output bins (= fftSize / 2 + 1).
    int getNumBins() const;

    /// Read a single bin magnitude (thread-safe via atomic).
    float getBin(int i) const;

    /// RMS magnitude over a bin range [lo, hi).
    float getBand(int lo, int hi) const;

    /// Time-domain RMS level.
    float getRMS() const;

    /// Atomic pointer for a single bin — used by External pattern.
    std::atomic<float>* getBinAtomic(int i);

    /// Atomic pointer for RMS — used by External pattern.
    std::atomic<float>* getRMSAtomic();

    /// Atomic pointer for a band RMS — lazily allocated, updated in update().
    std::atomic<float>* getBandAtomic(int lo, int hi);

private:
    /// A registered band that gets updated each frame from processor bin atomics.
    struct BandEntry {
        int lo, hi;
        std::atomic<float>* atomic;
    };

    std::unordered_map<uint64_t, std::unique_ptr<std::atomic<float>>> bandAtomics;
    std::vector<BandEntry> bandRanges;
};
