#pragma once
#include "ofSoundBuffer.h"
#include "NodeProcessor.h"
#include "kissfft/kiss_fftr.h"
#include <atomic>
#include <cmath>
#include <unordered_map>
#include <memory>
#include <vector>

namespace crumble {

/**
 * FFT: Spectral analysis capability for AudioProcessor.
 *
 * Not a node — a reusable analysis engine that sits inside any AudioProcessor.
 * After the processor's process() fills internalBuffer, accumulate() mixes to
 * mono, applies a Hann window, and runs kissfft when the buffer is full.
 * Results are exposed as atomic<float> per bin + RMS for External patterns.
 *
 * Band atomics (bass, mid, etc.) are computed lazily and refreshed each frame
 * from the main thread via refreshBands(), called from Node::update().
 *
 * Thread safety:
 *   - accumulate() called on audio thread (inside AudioProcessor::pull())
 *   - binAtomics/rmsAtomic written on audio thread, read by External patterns
 *   - bandAtomics written from main thread via refreshBands()
 *   - The FFT struct itself is created on main thread, installed via atomic
 *     pointer with release/acquire ordering — audio thread sees it after release
 */
class FFT {
public:
    static constexpr int MAX_FFT_SIZE = 4096;
    static constexpr int MAX_BINS = MAX_FFT_SIZE / 2 + 1;

    FFT() {
        for (int i = 0; i < MAX_BINS; i++) binAtomics[i].store(0.0f);
        rmsAtomic.store(0.0f);

        // Pre-compute Hann windows for all supported sizes
        for (int sz : {512, 1024, 2048, 4096}) {
            auto& win = hannWindows[sz];
            win.resize(sz);
            for (int i = 0; i < sz; i++) {
                win[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (sz - 1)));
            }
        }
    }

    ~FFT() {
        if (fftCfg) kiss_fftr_free(fftCfg);
    }

    /**
     * Feed audio samples into the windowed FFT accumulator.
     * Called on audio thread after process() fills internalBuffer.
     * Mixes to mono, applies Hann window, runs FFT when buffer is full.
     */
    void accumulate(const ofSoundBuffer& buffer, int fftSize, float smoothing) {
        fftSize = snapFFTSize(fftSize);
        smoothing = std::clamp(smoothing, 0.0f, 0.99f);

        // Reallocate kissfft if size changed
        if (fftSize != currentFFTSize) {
            if (fftCfg) kiss_fftr_free(fftCfg);
            fftCfg = kiss_fftr_alloc(fftSize, 0, nullptr, nullptr);
            currentFFTSize = fftSize;
            numBins = fftSize / 2 + 1;
            writePos = 0;
            std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
            fftOutBuffer.resize(numBins);
            for (int i = 0; i < MAX_BINS; i++) smoothedBins[i] = 0.0f;
            smoothedRMS = 0.0f;
        }

        int numChannels = buffer.getNumChannels();
        size_t numFrames = buffer.getNumFrames();
        const float* samples = buffer.getBuffer().data();

        for (size_t f = 0; f < numFrames; f++) {
            // Mix to mono for analysis
            float sample = 0.0f;
            for (int c = 0; c < numChannels; c++) {
                sample += samples[f * numChannels + c];
            }
            sample /= numChannels;

            // Apply Hann window and accumulate
            auto& win = hannWindows[currentFFTSize];
            fftBuffer[writePos] = sample * win[writePos];
            writePos++;

            if (writePos >= currentFFTSize) {
                runFFT(smoothing);
                writePos = 0;
            }
        }
    }

    int getNumBins() const { return numBins; }

    /// Atomic pointer for a single bin — used by External pattern.
    std::atomic<float>* getBinAtomic(int i) {
        if (i >= 0 && i < MAX_BINS) return &binAtomics[i];
        return nullptr;
    }

    /// Atomic pointer for RMS — used by External pattern.
    std::atomic<float>* getRMSAtomic() { return &rmsAtomic; }

    /**
     * Atomic pointer for a band RMS — lazily allocated.
     * Updated each frame in refreshBands() (main thread).
     */
    std::atomic<float>* getBandAtomic(int lo, int hi) {
        uint64_t key = ((uint64_t)lo << 32) | (uint64_t)hi;
        auto it = bandAtomics.find(key);
        if (it != bandAtomics.end()) return it->second.get();

        auto atomic = std::make_unique<std::atomic<float>>(0.0f);
        auto* ptr = atomic.get();
        bandAtomics[key] = std::move(atomic);
        bandRanges.push_back({lo, hi, ptr});
        return ptr;
    }

    /**
     * Refresh band atomics from bin atomics.
     * Called on main thread from Node::update().
     */
    void refreshBands() {
        for (auto& band : bandRanges) {
            int lo = std::max(0, band.lo);
            int hi = std::min(band.hi, numBins);
            if (lo >= hi) { band.atomic->store(0.0f); continue; }

            float sum = 0.0f;
            for (int i = lo; i < hi; i++) {
                sum += binAtomics[i].load(std::memory_order_acquire);
            }
            band.atomic->store(sqrtf(sum / (hi - lo)), std::memory_order_release);
        }
    }

private:
    /// Snap to nearest supported FFT size (must be power of 2, 512–4096).
    static int snapFFTSize(int requested) {
        int best = 2048;
        int bestDist = abs(requested - best);
        for (int sz : {512, 1024, 2048, 4096}) {
            int d = abs(requested - sz);
            if (d < bestDist) { bestDist = d; best = sz; }
        }
        return best;
    }

    void runFFT(float smoothing) {
        if (!fftCfg) return;

        kiss_fftr(fftCfg, fftBuffer.data(), fftOutBuffer.data());

        float rmsSum = 0.0f;
        for (int i = 0; i < numBins; i++) {
            float re = fftOutBuffer[i].r;
            float im = fftOutBuffer[i].i;
            float mag = sqrtf(re * re + im * im) / currentFFTSize;

            smoothedBins[i] = smoothing * smoothedBins[i] + (1.0f - smoothing) * mag;
            binAtomics[i].store(smoothedBins[i], std::memory_order_release);
            rmsSum += smoothedBins[i];
        }

        // Clear unused bins
        for (int i = numBins; i < MAX_BINS; i++) {
            binAtomics[i].store(0.0f, std::memory_order_release);
        }

        float rms = sqrtf(rmsSum / numBins);
        smoothedRMS = smoothing * smoothedRMS + (1.0f - smoothing) * rms;
        rmsAtomic.store(smoothedRMS, std::memory_order_release);
    }

    // Bin + RMS atomics (written on audio thread, read by External patterns)
    std::atomic<float> binAtomics[MAX_BINS];
    std::atomic<float> rmsAtomic;

    // Band atomics (lazily allocated, refreshed on main thread)
    struct BandEntry {
        int lo, hi;
        std::atomic<float>* atomic;
    };
    std::unordered_map<uint64_t, std::unique_ptr<std::atomic<float>>> bandAtomics;
    std::vector<BandEntry> bandRanges;

    // FFT state
    int currentFFTSize = 0;
    int numBins = 1025;
    int writePos = 0;

    kiss_fftr_cfg fftCfg = nullptr;
    std::vector<float> fftBuffer{std::vector<float>(MAX_FFT_SIZE, 0.0f)};
    std::vector<kiss_fft_cpx> fftOutBuffer;

    // Hann windows pre-computed for each supported FFT size
    std::unordered_map<int, std::vector<float>> hannWindows;

    float smoothedBins[MAX_BINS] = {};
    float smoothedRMS = 0.0f;
};

} // namespace crumble
