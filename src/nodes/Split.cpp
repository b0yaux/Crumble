#include "ofMain.h"
#include "Split.h"
#include "../core/NodeProcessor.h"

namespace crumble {

class SplitAudioProcessor : public AudioProcessor {
public:
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

        float* src = pullBuf.getBuffer().data();
        float* dst = buffer.getBuffer().data();
        for (size_t i = 0; i < pullBuf.size(); i++) {
            dst[i] += src[i];
        }
    }

private:
    ofSoundBuffer pullBuf;
};

class SplitVideoProcessor : public VideoProcessor {
public:
    ofTexture* getOutput(int index = 0) override {
        return cachedTex;
    }

    void processVideo(double cycle, double cycleStep) override {
        auto& input = inputs[0];
        if (!input.processor) { cachedTex = nullptr; return; }
        cachedTex = input.processor->getOutput(input.fromOutput);
    }

private:
    ofTexture* cachedTex = nullptr;
};

} // namespace crumble

Split::Split() {
    type = "split";
}

crumble::AudioProcessor* Split::createAudioProcessor() {
    return new crumble::SplitAudioProcessor();
}

crumble::VideoProcessor* Split::createVideoProcessor() {
    return new crumble::SplitVideoProcessor();
}
