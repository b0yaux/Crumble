#include "ofMain.h"
#include "FFT.h"
#include "../core/NodeProcessor.h"
#include "../core/kissfft/kiss_fftr.h"

#include <cmath>

namespace crumble {

/**
 * FFTAudioProcessor: Runs spectral analysis on audio passing through.
 *
 * Accumulates samples into a windowed buffer. When full, performs FFT via kissfft,
 * computes per-bin magnitudes with exponential smoothing, and writes results to
 * atomic floats. The main-thread FFT Node reads these to expose band-level atomics
 * for External pattern consumption.
 *
 * Passthrough — input copies to output unchanged.
 */
class FFTAudioProcessor : public AudioProcessor {
public:
    ControlSlot* sizeSlot = nullptr;
    ControlSlot* smoothingSlot = nullptr;

    static constexpr int MAX_FFT_SIZE = 4096;
    static constexpr int MAX_BINS = MAX_FFT_SIZE / 2 + 1;

    std::atomic<float> binAtomics[MAX_BINS];
    std::atomic<float> rmsAtomic;

    FFTAudioProcessor() {
        sizeSlot = getControlPtr(hashString("size"));
        smoothingSlot = getControlPtr(hashString("smoothing"));

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

    ~FFTAudioProcessor() {
        if (fftCfg) kiss_fftr_free(fftCfg);
    }

    void process(ofSoundBuffer& buffer, int index, uint64_t frameCounter,
                 double cycle, double cycleStep) override {
        auto& input = inputs[0];
        if (!input.processor) return;

        if (pullBuf.getNumFrames() != buffer.getNumFrames() ||
            pullBuf.getNumChannels() != buffer.getNumChannels()) {
            pullBuf.allocate(buffer.getNumFrames(), buffer.getNumChannels());
            pullBuf.setSampleRate(buffer.getSampleRate());
        }
        pullBuf.set(0);
        input.processor->pull(pullBuf, input.fromOutput, frameCounter, cycle, cycleStep);

        float sampleRate = buffer.getSampleRate();
        int numChannels = buffer.getNumChannels();
        size_t numFrames = buffer.getNumFrames();
        float* pIn = pullBuf.getBuffer().data();
        float* pOut = buffer.getBuffer().data();

        // Read size parameter (K-rate — evaluated once per buffer).
        // Round to nearest supported power of 2.
        int requestedSize = (int)evalSlot(sizeSlot, cycle);
        int fftSize = snapFFTSize(requestedSize);
        float smoothing = std::clamp(evalSlot(smoothingSlot, cycle), 0.0f, 0.99f);

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

        // Per-sample: accumulate into FFT buffer, passthrough audio
        for (size_t f = 0; f < numFrames; f++) {
            // Mix to mono for analysis
            float sample = 0.0f;
            for (int c = 0; c < numChannels; c++) {
                sample += pIn[f * numChannels + c];
                pOut[f * numChannels + c] = pIn[f * numChannels + c];
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

    ofSoundBuffer pullBuf;

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

// ---------------------------------------------------------------------------
// FFT Node implementation
// ---------------------------------------------------------------------------

FFT::FFT() {
    type = "fft";
    parameters->add(size.set("size", 2048, 512, 4096));
    parameters->add(smoothing.set("smoothing", 0.8, 0.0, 0.99));
}

crumble::AudioProcessor* FFT::createAudioProcessor() {
    return new crumble::FFTAudioProcessor();
}

int FFT::getNumBins() const {
    auto* p = dynamic_cast<crumble::FFTAudioProcessor*>(audioProcessor);
    return p ? p->getNumBins() : 0;
}

float FFT::getBin(int i) const {
    auto* p = dynamic_cast<crumble::FFTAudioProcessor*>(audioProcessor);
    if (p && i >= 0 && i < p->getNumBins())
        return p->binAtomics[i].load(std::memory_order_acquire);
    return 0.0f;
}

float FFT::getBand(int lo, int hi) const {
    auto* p = dynamic_cast<crumble::FFTAudioProcessor*>(audioProcessor);
    if (!p) return 0.0f;
    int nBins = p->getNumBins();
    lo = std::max(0, lo);
    hi = std::min(hi, nBins);
    if (lo >= hi) return 0.0f;
    float sum = 0.0f;
    for (int i = lo; i < hi; i++)
        sum += p->binAtomics[i].load(std::memory_order_acquire);
    return sqrtf(sum / (hi - lo));
}

float FFT::getRMS() const {
    auto* p = dynamic_cast<crumble::FFTAudioProcessor*>(audioProcessor);
    return p ? p->rmsAtomic.load(std::memory_order_acquire) : 0.0f;
}

std::atomic<float>* FFT::getBinAtomic(int i) {
    auto* p = dynamic_cast<crumble::FFTAudioProcessor*>(audioProcessor);
    if (p && i >= 0 && i < crumble::FFTAudioProcessor::MAX_BINS)
        return &p->binAtomics[i];
    return nullptr;
}

std::atomic<float>* FFT::getRMSAtomic() {
    auto* p = dynamic_cast<crumble::FFTAudioProcessor*>(audioProcessor);
    return p ? &p->rmsAtomic : nullptr;
}

std::atomic<float>* FFT::getBandAtomic(int lo, int hi) {
    // Band atomics are computed from bin atomics each frame in update().
    // Lazily allocate on first request.
    uint64_t key = ((uint64_t)lo << 32) | (uint64_t)hi;
    auto it = bandAtomics.find(key);
    if (it != bandAtomics.end()) return it->second.get();

    auto atomic = std::make_unique<std::atomic<float>>(0.0f);
    auto* ptr = atomic.get();
    bandAtomics[key] = std::move(atomic);
    bandRanges.push_back({lo, hi, ptr});
    return ptr;
}

void FFT::update(float dt) {
    Node::update(dt);

    // Refresh band atomics from processor bin atomics (main thread → main thread)
    auto* p = dynamic_cast<crumble::FFTAudioProcessor*>(audioProcessor);
    if (!p) return;

    int nBins = p->getNumBins();
    for (auto& band : bandRanges) {
        int lo = std::max(0, band.lo);
        int hi = std::min(band.hi, nBins);
        if (lo >= hi) { band.atomic->store(0.0f); continue; }

        float sum = 0.0f;
        for (int i = lo; i < hi; i++) {
            sum += p->binAtomics[i].load(std::memory_order_acquire);
        }
        band.atomic->store(sqrtf(sum / (hi - lo)), std::memory_order_release);
    }
}
