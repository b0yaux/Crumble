#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <memory>

/**
 * Generator is the base class for any mathematical pattern 
 * that can drive a Crumble Param<T>.
 */
template<typename T>
class Generator {
public:
    virtual ~Generator() = default;
    virtual T eval(double cycle) = 0;
};

/**
 * SeqGenerator: Parses Tidal-like strings ("1 0.5 -1") 
 * and returns the value for the current step.
 */
class SeqGenerator : public Generator<float> {
public:
    SeqGenerator(const std::string& pattern) {
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
 * OscGenerator: Sine wave LFO.
 */
class OscGenerator : public Generator<float> {
public:
    OscGenerator(float frequency) : freq(frequency) {}
    float eval(double cycle) override {
        return (std::sin(cycle * freq * 2.0 * M_PI) + 1.0f) * 0.5f;
    }
private:
    float freq;
};

/**
 * RampGenerator: Sawtooth LFO.
 */
class RampGenerator : public Generator<float> {
public:
    RampGenerator(float frequency) : freq(frequency) {}
    float eval(double cycle) override {
        double phase = cycle * freq;
        return (float)(phase - std::floor(phase));
    }
private:
    float freq;
};

/**
 * ConstGenerator: Returns a constant value.
 */
class ConstGenerator : public Generator<float> {
public:
    ConstGenerator(float value) : val(value) {}
    float eval(double cycle) override { return val; }
private:
    float val;
};

/**
 * MulGenerator: Multiplies two generators.
 */
class MulGenerator : public Generator<float> {
public:
    MulGenerator(std::shared_ptr<Generator<float>> a, std::shared_ptr<Generator<float>> b) : genA(a), genB(b) {}
    float eval(double cycle) override {
        return (genA ? genA->eval(cycle) : 0.0f) * (genB ? genB->eval(cycle) : 0.0f);
    }
private:
    std::shared_ptr<Generator<float>> genA, genB;
};

/**
 * AddGenerator: Adds two generators.
 */
class AddGenerator : public Generator<float> {
public:
    AddGenerator(std::shared_ptr<Generator<float>> a, std::shared_ptr<Generator<float>> b) : genA(a), genB(b) {}
    float eval(double cycle) override {
        return (genA ? genA->eval(cycle) : 0.0f) + (genB ? genB->eval(cycle) : 0.0f);
    }
private:
    std::shared_ptr<Generator<float>> genA, genB;
};
