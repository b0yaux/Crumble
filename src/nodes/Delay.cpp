#include "ofMain.h"
#include "Delay.h"
#include "../core/NodeProcessor.h"

namespace crumble {

class DelayAudioProcessor : public AudioProcessor {
public:
    ControlSlot* timeSlot = nullptr;
    ControlSlot* feedbackSlot = nullptr;
    ControlSlot* wetSlot = nullptr;

    static constexpr int MAX_DELAY_SAMPLES = 88200; // 2 seconds at 44100 Hz

    DelayAudioProcessor() {
        timeSlot = getControlPtr(hashString("time"));
        feedbackSlot = getControlPtr(hashString("feedback"));
        wetSlot = getControlPtr(hashString("wet"));
        ringBuffer.resize(MAX_DELAY_SAMPLES, 0.0f);
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

        for (size_t f = 0; f < numFrames; f++) {
            double sampleCycle = cycle + f * cycleStep;

            float time = evalSlot(timeSlot, sampleCycle);
            float feedback = evalSlot(feedbackSlot, sampleCycle);
            float wet = evalSlot(wetSlot, sampleCycle);

            time = std::max(0.0f, time);
            feedback = std::clamp(feedback, 0.0f, 0.98f);
            wet = std::clamp(wet, 0.0f, 1.0f);

            int delaySamples = static_cast<int>(time * sampleRate);
            delaySamples = std::min(delaySamples, MAX_DELAY_SAMPLES - 1);

            for (int c = 0; c < numChannels; c++) {
                float dry = pIn[f * numChannels + c];

                int readPos = writePos - delaySamples;
                if (readPos < 0) readPos += MAX_DELAY_SAMPLES;

                float delayed = ringBuffer[readPos];
                ringBuffer[writePos] = dry + feedback * delayed;

                pOut[f * numChannels + c] = dry * (1.0f - wet) + delayed * wet;
            }

            writePos = (writePos + 1) % MAX_DELAY_SAMPLES;
        }
    }

private:
    ofSoundBuffer pullBuf;
    std::vector<float> ringBuffer;
    int writePos = 0;
};

} // namespace crumble

Delay::Delay() {
    type = "delay";
    parameters->add(time.set("time", 0.3, 0.0, 2.0));
    parameters->add(feedback.set("feedback", 0.5, 0.0, 0.98));
    parameters->add(wet.set("wet", 0.5, 0.0, 1.0));
}

crumble::AudioProcessor* Delay::createAudioProcessor() {
    return new crumble::DelayAudioProcessor();
}
