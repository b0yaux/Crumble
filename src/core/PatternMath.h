#pragma once
#include "Patterns.h"
#include <algorithm>

/**
 * PatternMath: High-level functional decorators for the Pattern system.
 * These classes wrap existing patterns to transform their time or value
 * without storing state, maintaining sample-accurate precision.
 */
namespace patterns {

    // --- Time Transformations ---

    /**
     * Speed: Squeezes or stretches time (Fast/Slow).
     */
    class Speed : public Pattern<float> {
    public:
        Speed(float factor, std::shared_ptr<Pattern<float>> p) : f(factor), pat(p) {}
        float eval(double cycle) override {
            return pat ? pat->eval(cycle * f) : 0.0f;
        }
        std::string getSignature() const override { 
            return "speed:" + std::to_string(f) + "(" + (pat ? pat->getSignature() : "null") + ")"; 
        }
    private:
        float f;
        std::shared_ptr<Pattern<float>> pat;
    };

    /**
     * Shift: Offsets the phase of a pattern.
     */
    class Shift : public Pattern<float> {
    public:
        Shift(float amount, std::shared_ptr<Pattern<float>> p) : o(amount), pat(p) {}
        float eval(double cycle) override {
            return pat ? pat->eval(cycle + o) : 0.0f;
        }
        std::string getSignature() const override { 
            return "shift:" + std::to_string(o) + "(" + (pat ? pat->getSignature() : "null") + ")"; 
        }
    private:
        float o;
        std::shared_ptr<Pattern<float>> pat;
    };

    // --- Value Transformations ---

    /**
     * Scale: Maps the standard [0, 1] output of a pattern to [min, max].
     */
    class Scale : public Pattern<float> {
    public:
        Scale(float min, float max, std::shared_ptr<Pattern<float>> p) : lo(min), hi(max), pat(p) {}
        float eval(double cycle) override {
            if (!pat) return lo;
            float val = pat->eval(cycle);
            return lo + val * (hi - lo);
        }
        std::string getSignature() const override { 
            return "scale:" + std::to_string(lo) + "," + std::to_string(hi) + "(" + (pat ? pat->getSignature() : "null") + ")"; 
        }
    private:
        float lo, hi;
        std::shared_ptr<Pattern<float>> pat;
    };

    /**
     * Snap: Quantizes the value into discrete steps (Staircase).
     */
    class Snap : public Pattern<float> {
    public:
        Snap(float steps, std::shared_ptr<Pattern<float>> p) : n(steps), pat(p) {}
        float eval(double cycle) override {
            if (!pat) return 0.0f;
            float val = pat->eval(cycle);
            if (n <= 0) return val;
            return std::round(val * n) / n;
        }
        std::string getSignature() const override { 
            return "snap:" + std::to_string(n) + "(" + (pat ? pat->getSignature() : "null") + ")"; 
        }
    private:
        float n;
        std::shared_ptr<Pattern<float>> pat;
    };

    // --- Combinators ---

    /**
     * Sum: Adds two patterns.
     */
    class Sum : public Pattern<float> {
    public:
        Sum(std::shared_ptr<Pattern<float>> a, std::shared_ptr<Pattern<float>> b) : patA(a), patB(b) {}
        float eval(double cycle) override {
            return (patA ? patA->eval(cycle) : 0.0f) + (patB ? patB->eval(cycle) : 0.0f);
        }
        std::string getSignature() const override { 
            return "sum(" + (patA ? patA->getSignature() : "null") + "," + (patB ? patB->getSignature() : "null") + ")"; 
        }
    private:
        std::shared_ptr<Pattern<float>> patA, patB;
    };

    /**
     * Product: Multiplies two patterns.
     */
    class Product : public Pattern<float> {
    public:
        Product(std::shared_ptr<Pattern<float>> a, std::shared_ptr<Pattern<float>> b) : patA(a), patB(b) {}
        float eval(double cycle) override {
            return (patA ? patA->eval(cycle) : 0.0f) * (patB ? patB->eval(cycle) : 0.0f);
        }
        std::string getSignature() const override { 
            return "prod(" + (patA ? patA->getSignature() : "null") + "," + (patB ? patB->getSignature() : "null") + ")"; 
        }
    private:
        std::shared_ptr<Pattern<float>> patA, patB;
    };

}
