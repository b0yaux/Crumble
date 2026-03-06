#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <memory>

/**
 * Pattern: A stateless mathematical mapping of cycle (0.0-1.0) to value.
 * Patterns are sampled at any rate (e.g. audio rate or video rate)
 * and remain perfectly phase-aligned via double-precision cycle evaluation.
 */
template<typename T>
class Pattern {
public:
    virtual ~Pattern() = default;
    
    /**
     * Evaluates the pattern at a specific phase in the musical timeline.
     * @param cycle The absolute phase (0.0 to 1.0 per bar/loop).
     */
    virtual T eval(double cycle) = 0;

    /**
     * Returns a unique signature string representing the pattern's content.
     * Used for idempotency optimization to avoid redundant re-parsing.
     */
    virtual std::string getSignature() const = 0;
};

namespace patterns {

    /**
     * Constant: A static value.
     */
    class Constant : public Pattern<float> {
    public:
        Constant(float value) : val(value) {}
        float eval(double cycle) override { return val; }
        std::string getSignature() const override { return "const:" + std::to_string(val); }
    private:
        float val;
    };

    /**
     * Osc: Sine wave generator.
     * @param freq Frequency in Cycles-Per-Bar (1.0 = repeats once per bar).
     */
    class Osc : public Pattern<float> {
    public:
        Osc(float frequency = 1.0f) : freq(frequency) {}
        float eval(double cycle) override {
            return (std::sin(cycle * freq * 2.0 * M_PI) + 1.0f) * 0.5f;
        }
        std::string getSignature() const override { return "osc:" + std::to_string(freq); }
    private:
        float freq;
    };

    /**
     * Ramp: Sawtooth generator (0 to 1).
     * @param freq Frequency in Cycles-Per-Bar.
     */
    class Ramp : public Pattern<float> {
    public:
        Ramp(float frequency = 1.0f) : freq(frequency) {}
        float eval(double cycle) override {
            double phase = cycle * freq;
            return (float)(phase - std::floor(phase));
        }
        std::string getSignature() const override { return "ramp:" + std::to_string(freq); }
    private:
        float freq;
    };

    /**
     * Noise: Stateless deterministic hash noise.
     */
    class Noise : public Pattern<float> {
    public:
        Noise(float seed = 0.0f) : s(seed) {}
        float eval(double cycle) override {
            double x = std::sin(cycle + s) * 43758.5453123;
            return (float)(x - std::floor(x));
        }
        std::string getSignature() const override { return "noise:" + std::to_string(s); }
    private:
        float s;
    };

    /**
     * Seq: Discrete step sequencer.
     * Parses a string of numbers (e.g., "1 0 0.5 -1").
     */
    class Seq : public Pattern<float> {
    public:
        Seq(const std::string& patternString) : raw(patternString) {
            std::stringstream ss(patternString);
            std::string token;
            while (ss >> token) {
                try { steps.push_back(std::stof(token)); }
                catch (...) { steps.push_back(0.0f); }
            }
        }

        float eval(double cycle) override {
            if (steps.empty()) return 0.0f;
            double phase = cycle - std::floor(cycle);
            int index = (int)(phase * steps.size());
            index = std::max(0, std::min((int)steps.size() - 1, index));
            return steps[index];
        }

        std::string getSignature() const override { return "seq:" + raw; }

    private:
        std::string raw;
        std::vector<float> steps;
    };
}

