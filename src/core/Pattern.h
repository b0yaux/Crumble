#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <memory>

/**
 * Pattern is the base class for any mathematical shape over time.
 * It is a stateless function: cycle -> value.
 */
template<typename T>
class Pattern {
public:
    virtual ~Pattern() = default;
    virtual T eval(double cycle) = 0;
};

/**
 * Concrete implementations of Pattern logic.
 */
namespace patterns {

    /**
     * Seq: Parses strings ("1 0.5 -1") 
     * and returns the value for the current step.
     */
    class Seq : public Pattern<float> {
    public:
        Seq(const std::string& pattern) {
            std::stringstream ss(pattern);
            std::string token;
            while (ss >> token) {
                try {
                    steps.push_back(std::stof(token));
                } catch (...) {
                    steps.push_back(0.0f);
                }
            }
        }

        float eval(double cycle) override {
            if (steps.empty()) return 0.0f;
            
            // Normalize cycle to [0, 1)
            double phase = cycle - std::floor(cycle);
            
            int index = (int)(phase * steps.size());
            if (index >= (int)steps.size()) index = (int)steps.size() - 1;
            if (index < 0) index = 0;
            
            return steps[index];
        }

    private:
        std::vector<float> steps;
    };

    /**
     * Osc: Sine wave shape.
     */
    class Osc : public Pattern<float> {
    public:
        Osc(float frequency) : freq(frequency) {}
        float eval(double cycle) override {
            return (std::sin(cycle * freq * 2.0 * M_PI) + 1.0f) * 0.5f;
        }
    private:
        float freq;
    };

    /**
     * Ramp: Sawtooth shape.
     */
    class Ramp : public Pattern<float> {
    public:
        Ramp(float frequency) : freq(frequency) {}
        float eval(double cycle) override {
            double phase = cycle * freq;
            return (float)(phase - std::floor(phase));
        }
    private:
        float freq;
    };

    /**
     * Constant: Returns a constant value.
     */
    class Constant : public Pattern<float> {
    public:
        Constant(float value) : val(value) {}
        float eval(double cycle) override { return val; }
    private:
        float val;
    };

    /**
     * Mul: Multiplies two patterns.
     */
    class Mul : public Pattern<float> {
    public:
        Mul(std::shared_ptr<Pattern<float>> a, std::shared_ptr<Pattern<float>> b) : patA(a), patB(b) {}
        float eval(double cycle) override {
            return (patA ? patA->eval(cycle) : 0.0f) * (patB ? patB->eval(cycle) : 0.0f);
        }
    private:
        std::shared_ptr<Pattern<float>> patA, patB;
    };

    /**
     * Add: Adds two patterns.
     */
    class Add : public Pattern<float> {
    public:
        Add(std::shared_ptr<Pattern<float>> a, std::shared_ptr<Pattern<float>> b) : patA(a), patB(b) {}
        float eval(double cycle) override {
            return (patA ? patA->eval(cycle) : 0.0f) + (patB ? patB->eval(cycle) : 0.0f);
        }
    private:
        std::shared_ptr<Pattern<float>> patA, patB;
    };
}
