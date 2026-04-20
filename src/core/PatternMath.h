#pragma once
#include "Patterns.h"
#include <algorithm>

/**
 * PatternMath: High-level functional decorators for the Pattern system.
 * These classes wrap existing patterns to transform their time or value
 * without storing state, maintaining sample-accurate precision.
 *
 * NOTE (node chaining): sampler("k s"):fast(4) was deliberately NOT implemented.
 * It would require hidden state (_pathGen) on nodes to intercept gen table
 * method calls, coupling makeSampler to NodeMeta.__index with special-casing
 * for :scale(). The accepted form is sampler(seq("k s"):fast(8)) — gen table
 * chaining happens before the sampler sees it. Honest, no hidden state.
 */
namespace patterns {

    // --- Time Transformations ---

    /**
     * Density: Squeezes or stretches pattern time (Fast/Slow).
     * Accepts either constant factor or dynamic pattern.
     */
    class Density : public Pattern<float> {
    public:
        // Constructor for constant factor (backward compatible)
        Density(float factor, std::shared_ptr<Pattern<float>> p) 
            : densityPat(nullptr), constantFactor(factor), pat(p) {}
        
        // Constructor for dynamic pattern factor
        Density(std::shared_ptr<Pattern<float>> factorPat, std::shared_ptr<Pattern<float>> p)
            : densityPat(factorPat), constantFactor(1.0f), pat(p) {}
        
        std::vector<Event<float>> query(double start, double end) override {
            if (!pat) return {};
            double factor = densityPat ? std::max(0.1f, densityPat->eval(start)) : constantFactor;
            double innerStart = start * factor;
            double innerEnd = end * factor;

            auto events = pat->query(innerStart, innerEnd);

            for (auto& e : events) {
                // Rescale onset from inner cycle phase to outer cycle phase.
                // Inner onset is in [0,1) relative to the inner cycle it belongs to.
                // We disambiguate which inner cycle using the inner query boundaries:
                //   - If onset >= inner cycle phase of innerStart → same inner cycle as query start
                //   - If onset < inner cycle phase of innerStart → next inner cycle (wraparound)
                double innerPhase = innerStart - std::floor(innerStart);
                double innerCycleNum;
                if (e.onset >= innerPhase) {
                    innerCycleNum = std::floor(innerStart);
                } else {
                    innerCycleNum = std::floor(innerStart) + 1.0;
                }
                double outerAbs = (innerCycleNum + e.onset) / factor;
                e.onset = outerAbs - std::floor(outerAbs);
                if (e.duration > 0) {
                    e.duration /= factor;
                }
            }

            return events;
        }
        
        float eval(double cycle) override {
            if (!pat) return 0.0f;
            if (densityPat) {
                // Dynamic density: multiply current cycle by evaluated speed
                float speed = std::max(0.1f, densityPat->eval(cycle));
                return pat->eval(cycle * speed);
            } else {
                // Constant density
                return pat->eval(cycle * constantFactor);
            }
        }
        
        std::string getSignature() const override { 
            if (densityPat) {
                return "density:pat(" + (pat ? pat->getSignature() : "null") + ")";
            } else {
                return "density:" + std::to_string(constantFactor) + "(" + (pat ? pat->getSignature() : "null") + ")";
            }
        }
    private:
        std::shared_ptr<Pattern<float>> densityPat;  // Dynamic density pattern
        float constantFactor;                         // Constant density factor
        std::shared_ptr<Pattern<float>> pat;
    };

    /**
     * Shift: Offsets the phase of a pattern.
     */
    class Shift : public Pattern<float> {
    public:
        Shift(float amount, std::shared_ptr<Pattern<float>> p) : o(amount), pat(p) {}
        
        std::vector<Event<float>> query(double start, double end) override {
            if (!pat) return {};
            return pat->query(start + o, end + o);
        }
        
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
        
        std::vector<Event<float>> query(double start, double end) override {
            if (!pat) return {};
            auto events = pat->query(start, end);
            for (auto& e : events) {
                e.value = lo + e.value * (hi - lo);
            }
            return events;
        }
        
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
        
        std::vector<Event<float>> query(double start, double end) override {
            if (!pat) return {};
            auto events = pat->query(start, end);
            for (auto& e : events) {
                if (n > 0) e.value = std::round(e.value * n) / n;
            }
            return events;
        }
        
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

    /**
     * Abs: Returns the absolute value of a pattern.
     * Useful for symmetric gamepad axis handling.
     */
    class Abs : public Pattern<float> {
    public:
        Abs(std::shared_ptr<Pattern<float>> p) : pat(p) {}
        
        std::vector<Event<float>> query(double start, double end) override {
            if (!pat) return {};
            auto events = pat->query(start, end);
            for (auto& e : events) {
                e.value = std::abs(e.value);
            }
            return events;
        }
        
        float eval(double cycle) override {
            if (!pat) return 0.0f;
            return std::abs(pat->eval(cycle));
        }
        std::string getSignature() const override { 
            return "abs(" + (pat ? pat->getSignature() : "null") + ")"; 
        }
    private:
        std::shared_ptr<Pattern<float>> pat;
    };

    /**
     * Pow: Power curve. Applies sign(x) * |x|^p to reshape response.
     *
     *   p < 1 (e.g. 0.5): sqrt — more resolution near zero (end, gain)
     *   p > 1 (e.g. 2.0): quadratic — more resolution near extremes (speed)
     *   p = 1.0: linear identity
     *
     * Sign-preserving, so it works on both unipolar [0,1] and bipolar [-1,1] sources.
     * Composes with any source: gamepad, MIDI, LFOs, noise.
     *
     *   gpad("ry"):accum(0.3, 0.5):pow(0.5):scale(0.001, 1)  — fine grain control
     *   gpad("ly"):accum(0.5, 0.5):pow(2.0):scale(-3, 3)     — aggressive speed
     */
    class Pow : public Pattern<float> {
    public:
        Pow(float exponent, std::shared_ptr<Pattern<float>> p) : e(exponent), pat(p) {}

        std::vector<Event<float>> query(double start, double end) override {
            if (!pat) return {};
            auto events = pat->query(start, end);
            for (auto& ev : events) {
                ev.value = applyCurve(ev.value);
            }
            return events;
        }

        float eval(double cycle) override {
            if (!pat) return 0.0f;
            return applyCurve(pat->eval(cycle));
        }

        std::string getSignature() const override {
            return "pow:" + std::to_string(e) + "(" + (pat ? pat->getSignature() : "null") + ")";
        }
    private:
        float e;
        std::shared_ptr<Pattern<float>> pat;

        float applyCurve(float v) const {
            if (e == 1.0f) return v;
            float sign = (v >= 0.0f) ? 1.0f : -1.0f;
            return sign * std::pow(std::abs(v), e);
        }
    };

    // --- Combinators ---

    /**
     * Sum: Adds two patterns.
     */
    class Sum : public Pattern<float> {
    public:
        Sum(std::shared_ptr<Pattern<float>> a, std::shared_ptr<Pattern<float>> b) : patA(a), patB(b) {}
        
        std::vector<Event<float>> query(double start, double end) override {
            // For combination, we take events from both patterns and merge
            auto eventsA = patA ?patA->query(start, end) : std::vector<Event<float>>{};
            auto eventsB = patB ? patB->query(start, end) : std::vector<Event<float>>{};
            
            // Simple merge: add values at same onset
            std::vector<Event<float>> result = eventsA;
            for (auto& eb : eventsB) {
                bool found = false;
                for (auto& er : result) {
                    if (std::abs(er.onset - eb.onset) < 0.0001) {
                        er.value += eb.value;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    result.push_back(eb);
                }
            }
            return result;
        }
        
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
        
        std::vector<Event<float>> query(double start, double end) override {
            // For product, we combine events multiplicatively
            auto eventsA = patA ? patA->query(start, end) : std::vector<Event<float>>{};
            auto eventsB = patB ? patB->query(start, end) : std::vector<Event<float>>{};
            
            // Use eval for multiplicative combination (simplified)
            std::vector<Event<float>> result;
            for (auto& ea : eventsA) {
                Event<float> e = ea;
                e.value *= patB ? patB->eval(ea.onset) : 0.0f;
                result.push_back(e);
            }
            return result;
        }
        
        float eval(double cycle) override {
            return (patA ? patA->eval(cycle) : 0.0f) * (patB ? patB->eval(cycle) : 0.0f);
        }
        std::string getSignature() const override { 
            return "prod(" + (patA ? patA->getSignature() : "null") + "," + (patB ? patB->getSignature() : "null") + ")"; 
        }
    private:
        std::shared_ptr<Pattern<float>> patA, patB;
    };

    // --- Stateful Primitives ---

    /**
     * Accum: Velocity integrator. Accumulates the source pattern's value
     * over time at a given rate.
     *
     * output(t) = output(t-1) + source(t) * rate * dt
     *
     * General-purpose stateful pattern. Composes with any source:
     *   gpad("rx"):accum(0.5)   — gamepad stick as velocity (navigate a slider)
     *   noise(2):accum(0.1)     — random walk (Brownian motion)
     *   midi(1,10):accum(0.3)   — MIDI CC as velocity
     *   osc(0.5):accum(0.2)     — oscillator-driven ramp
     *
     * Wraps [0,1] when wrap=true, clamps otherwise.
     *
     * Thread safety: Audio-thread-only eval. No synchronization needed.
     * Clone requirement: Must be unique per slot (not shared across nodes).
     */
    class Accum : public Pattern<float> {
    public:
        Accum(float rate, std::shared_ptr<Pattern<float>> source, float initial = 0.0f, bool doWrap = false)
            : r(rate), wrap(doWrap), pat(source), output(initial) {}

        std::vector<Event<float>> query(double start, double end) override {
            return {Event<float>(eval(start), start, end - start, false)};
        }

        float eval(double cycle) override {
            if (!pat) return output;
            double dt = (lastCycle >= 0.0) ? std::max(0.0, cycle - lastCycle) : 0.0;
            lastCycle = cycle;
            float v = pat->eval(cycle);
            output += v * r * (float)dt;
            if (wrap) {
                output = output - std::floor(output);
            } else {
                output = std::max(0.0f, std::min(1.0f, output));
            }
            return output;
        }

        std::string getSignature() const override {
            return "accum:" + std::to_string(r) + "(" + (pat ? pat->getSignature() : "null") + ")";
        }
    private:
        float r;
        bool wrap;
        std::shared_ptr<Pattern<float>> pat;
        mutable float output = 0.0f;
        mutable double lastCycle = -1.0;
    };

    /**
     * Smooth: Exponential slew limiter (one-pole low-pass filter).
     *
     * output(t) = output(t-1) + (source(t) - output(t-1)) * alpha
     * alpha = 1 - exp(-dt / tau)
     *
     * tau is in bars. Small tau = fast response. Large tau = sluggish.
     * General-purpose: smooths any pattern's transitions — gamepads, LFOs,
     * sequencer steps, MIDI controllers.
     *
     * Thread safety: Audio-thread-only eval. No synchronization needed.
     * Clone requirement: Must be unique per slot (not shared across nodes).
     */
    class Smooth : public Pattern<float> {
    public:
        Smooth(float tau, std::shared_ptr<Pattern<float>> source)
            : t(tau), pat(source) {}

        std::vector<Event<float>> query(double start, double end) override {
            return {Event<float>(eval(start), start, end - start, false)};
        }

        float eval(double cycle) override {
            if (!pat) return output;
            double dt = (lastCycle >= 0.0) ? std::max(0.0, cycle - lastCycle) : 0.0;
            lastCycle = cycle;
            float target = pat->eval(cycle);
            if (t <= 0.0f || dt <= 0.0) {
                output = target;
            } else {
                float alpha = 1.0f - std::exp(-(float)dt / t);
                output += (target - output) * alpha;
            }
            return output;
        }

        std::string getSignature() const override {
            return "smooth:" + std::to_string(t) + "(" + (pat ? pat->getSignature() : "null") + ")";
        }
    private:
        float t;
        std::shared_ptr<Pattern<float>> pat;
        mutable float output = 0.0f;
        mutable double lastCycle = -1.0;
    };

    /**
     * Toggle: Rising-edge state machine. Flips between 0 and 1
     * when the source pattern crosses above the threshold.
     * Resets (arms for next flip) when source drops below threshold.
     *
     * Follows PD's threshold~ / SC's Schmidt model:
     * configurable threshold with hysteresis (rise at threshold, arm on fall).
     *
     * Composes with any source:
     *   gpad("triangle"):toggle()        — button at default 0.5 threshold
     *   gpad("triangle"):toggle(0.1)     — low threshold (any touch triggers)
     *   noise(2):toggle(0.7)             — noise-driven toggle
     *
     * Thread safety: Audio-thread-only eval. No synchronization needed.
     * Clone requirement: Must be unique per slot (not shared across nodes).
     */
    class Toggle : public Pattern<float> {
    public:
        Toggle(std::shared_ptr<Pattern<float>> source, float thresh = 0.5f)
            : pat(source), threshold(thresh) {}

        std::vector<Event<float>> query(double start, double end) override {
            return {Event<float>(eval(start), start, end - start, false)};
        }

        float eval(double cycle) override {
            if (!pat) return state ? 1.0f : 0.0f;
            float v = pat->eval(cycle);
            if (v >= threshold && !wasHigh) {
                state = !state;
            }
            wasHigh = v >= threshold;
            return state ? 1.0f : 0.0f;
        }

        std::string getSignature() const override {
            return "toggle:" + std::to_string(threshold) + "(" + (pat ? pat->getSignature() : "null") + ")";
        }
    private:
        std::shared_ptr<Pattern<float>> pat;
        float threshold;
        mutable bool state = false;
        mutable bool wasHigh = false;
    };

}