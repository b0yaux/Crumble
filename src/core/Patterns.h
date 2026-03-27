#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <algorithm>
#include <optional>

/**
 * Event: A single value with temporal metadata.
 * Used for discrete events (triggers) and continuous sampling.
 */
template<typename T>
struct Event {
    T value;                    // The actual value
    double onset;              // When this event starts (0.0-1.0 cycle)
    double duration;           // How long it lasts (0.0-1.0 cycle)
    bool isRest = false;       // Is this a rest?
    
    Event(T v, double o, double d = 0.0, bool r = false)
        : value(v), onset(o), duration(d), isRest(r) {}
    
    // Check if a cycle position falls within this event
    bool contains(double cycle) const {
        return cycle >= onset && cycle < onset + duration;
    }
};

/**
 * Pattern: A stateless function from Time to Events.
 * 
 * Tidal/Strudel model: A pattern is a query function that returns
 * all events with onsets in a given time range.
 * 
 * This supports:
 * - Discrete patterns (sequencers, triggers) with specific onset times
 * - Continuous patterns (LFOs, oscillators) sampled at any point
 * - Pattern composition (speed, scale, shift)
 */
template<typename T>
class Pattern {
public:
    virtual ~Pattern() = default;
    
    /**
     * Query the pattern for events in a time range.
     * This is the primary interface - returns all events whose onsets
     * fall within [start, end).
     * 
     * @param start Start of query arc (0.0-1.0 cycle)
     * @param end End of query arc (0.0-1.0 cycle)
     * @return Vector of events with onsets in the range
     */
    virtual std::vector<Event<T>> query(double start, double end) = 0;
    
    /**
     * Evaluates the pattern at a specific cycle.
     * For continuous patterns (LFOs), returns the interpolated value.
     * For discrete patterns, returns the value of the active event.
     * 
     * Default implementation queries [cycle, cycle+epsilon) and returns
     * the first event's value, or T{} if no events.
     */
    virtual T eval(double cycle) {
        auto events = query(cycle, cycle + 0.0001);
        if (!events.empty()) {
            return events[0].isRest ? T{} : events[0].value;
        }
        return T{};
    }

    /**
     * Returns a unique signature string representing the pattern's content.
     * Used for idempotency optimization to avoid redundant re-parsing.
     */
    virtual std::string getSignature() const = 0;
};

namespace patterns {

    /**
     * Constant: A static value spanning the entire cycle.
     */
    class Constant : public Pattern<float> {
    public:
        Constant(float value) : val(value) {}
        
        std::vector<Event<float>> query(double start, double end) override {
            // Constant covers entire cycle, so return if range is non-empty
            if (end > start) {
                return {Event<float>(val, 0.0, 1.0, false)};
            }
            return {};
        }
        
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
        
        std::vector<Event<float>> query(double start, double end) override {
            // Continuous pattern - return value at start of query
            float val = eval(start);
            return {Event<float>(val, start, end - start, false)};
        }
        
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
        
        std::vector<Event<float>> query(double start, double end) override {
            // Continuous pattern - return value at start of query
            float val = eval(start);
            return {Event<float>(val, start, end - start, false)};
        }
        
        float eval(double cycle) override {
            double phase = cycle * freq;
            return (float)(phase - std::floor(phase));
        }
        std::string getSignature() const override { return "ramp:" + std::to_string(freq); }
    private:
        float freq;
    };

    /**
     * Noise: Smooth deterministic gradient noise.
     */
    class Noise : public Pattern<float> {
    public:
        Noise(float frequency = 1.0f, float seed = 0.0f) : f(frequency), s(seed) {}
        
        std::vector<Event<float>> query(double start, double end) override {
            // Continuous pattern - return value at start of query
            float val = eval(start);
            return {Event<float>(val, start, end - start, false)};
        }
        
        float eval(double cycle) override {
            double x = cycle * f + s;
            int x0 = (int)std::floor(x);
            int x1 = x0 + 1;
            float t = (float)(x - x0);
            float st = t * t * (3.0f - 2.0f * t); // Smoothstep
            return (1.0f - st) * hash(x0) + st * hash(x1);
        }
        std::string getSignature() const override { return "noise:" + std::to_string(f) + "," + std::to_string(s); }
    private:
        float hash(int i) {
            double x = std::sin(i * 12.9898 + s * 78.233) * 43758.5453123;
            return (float)(x - std::floor(x));
        }
        float f, s;
    };

    /**
     * REST sentinel value for mini-notation rests (~)
     */
    static constexpr float REST_VALUE = -999.0f;
    inline bool isRest(float v) { return v <= REST_VALUE + 0.1f && v >= REST_VALUE - 0.1f; }

    /**
     * Seq: Discrete step sequencer with mini-notation support.
     * 
     * Mini-notation syntax (Tidal/Strudel-style):
     *   - "0 1 2 3"      : Simple steps
     *   - "~"            : Rest (returns REST_VALUE)
     *   - "0 ~ 1 ~"      : Steps with rests
     *   - "[0 1] 2"      : Subdivision (0 and 1 share one step)
     *   - "0*2 1"        : Repetition (0 plays twice)
     *   - "bd sd hh"     : Named samples (resolved to indices)
     */
    class Seq : public Pattern<float> {
    public:
        Seq(const std::string& patternString, const std::string& bankName = "") 
            : raw(patternString), bank(bankName) {
            parseMiniNotation(patternString);
        }

        std::vector<Event<float>> query(double start, double end) override {
            std::vector<Event<float>> events;
            if (steps.empty()) return events;
            
            // Normalize to cycle phase [0, 1)
            double cycleStart = start - std::floor(start);
            double cycleEnd = end - std::floor(end);
            
            // Handle wraparound
            if (end < start) {
                // Query crosses cycle boundary
                auto firstHalf = queryRange(cycleStart, 1.0);
                auto secondHalf = queryRange(0.0, cycleEnd);
                events.insert(events.end(), firstHalf.begin(), firstHalf.end());
                events.insert(events.end(), secondHalf.begin(), secondHalf.end());
            } else {
                events = queryRange(cycleStart, cycleEnd);
            }
            
            return events;
        }

        float eval(double cycle) override {
            if (steps.empty()) return 0.0f;
            double phase = cycle - std::floor(cycle);
            int index = (int)(phase * steps.size());
            index = std::max(0, std::min((int)steps.size() - 1, index));
            return steps[index];
        }

        std::string getSignature() const override { return "seq:" + raw + ":" + bank; }
        
        const std::vector<float>& getSteps() const { return steps; }
        bool isStepRest(int index) const {
            if (index < 0 || index >= (int)steps.size()) return true;
            return isRest(steps[index]);
        }

    private:
        std::string raw;
        std::string bank;
        std::vector<float> steps;

        std::vector<Event<float>> queryRange(double cycleStart, double cycleEnd) {
            std::vector<Event<float>> events;
            if (steps.empty()) return events;
            
            double stepDuration = 1.0 / steps.size();
            
            for (int i = 0; i < (int)steps.size(); ++i) {
                double onset = i * stepDuration;
                if (onset >= cycleStart && onset < cycleEnd) {
                    float val = steps[i];
                    bool rest = isRest(val);
                    events.emplace_back(val, onset, stepDuration, rest);
                }
            }
            
            return events;
        }

        void parseMiniNotation(const std::string& input) {
            std::vector<std::string> tokens = tokenize(input);
            
            for (size_t i = 0; i < tokens.size(); ++i) {
                const std::string& token = tokens[i];
                
                if (token == "~") {
                    // Rest
                    steps.push_back(REST_VALUE);
                } else if (token.front() == '[' && token.back() == ']') {
                    // Subdivision: [0 1 2]
                    // For now, flatten subdivisions into sequential steps
                    std::string inner = token.substr(1, token.size() - 2);
                    std::vector<std::string> subTokens = tokenize(inner);
                    for (const auto& sub : subTokens) {
                        if (sub == "~") {
                            steps.push_back(REST_VALUE);
                        } else {
                            float val = parseValue(sub);
                            steps.push_back(val);
                        }
                    }
                } else if (token.find('*') != std::string::npos) {
                    // Repetition: 0*2 or bd*3
                    size_t starPos = token.find('*');
                    std::string baseToken = token.substr(0, starPos);
                    int count = 1;
                    try {
                        count = std::stoi(token.substr(starPos + 1));
                    } catch (...) {}
                    
                    float val = parseValue(baseToken);
                    for (int r = 0; r < count; ++r) {
                        steps.push_back(val);
                    }
                } else if (token.find(':') != std::string::npos) {
                    // Bank:index notation: drums:0 or :0
                    size_t colonPos = token.find(':');
                    std::string explicitBank = token.substr(0, colonPos);
                    std::string indexStr = token.substr(colonPos + 1);
                    
                    if (!explicitBank.empty()) {
                        bank = explicitBank;
                    }
                    
                    try {
                        steps.push_back(std::stof(indexStr));
                    } catch (...) {
                        steps.push_back(0.0f);
                    }
                } else {
                    // Regular value or named sample
                    steps.push_back(parseValue(token));
                }
            }
        }

        std::vector<std::string> tokenize(const std::string& input) {
            std::vector<std::string> tokens;
            std::string current;
            int bracketDepth = 0;
            
            for (char c : input) {
                if (c == '[') {
                    if (!current.empty() && bracketDepth == 0) {
                        tokens.push_back(trim(current));
                        current.clear();
                    }
                    bracketDepth++;
                    current += c;
                } else if (c == ']') {
                    bracketDepth--;
                    current += c;
                    if (bracketDepth == 0) {
                        tokens.push_back(trim(current));
                        current.clear();
                    }
                } else if (std::isspace(c) && bracketDepth == 0) {
                    if (!current.empty()) {
                        tokens.push_back(trim(current));
                        current.clear();
                    }
                } else {
                    current += c;
                }
            }
            
            if (!current.empty()) {
                tokens.push_back(trim(current));
            }
            
            return tokens;
        }

        std::string trim(const std::string& s) {
            size_t start = 0;
            while (start < s.size() && std::isspace(s[start])) start++;
            size_t end = s.size();
            while (end > start && std::isspace(s[end - 1])) end--;
            return s.substr(start, end - start);
        }

        float parseValue(const std::string& token) {
            // Try to parse as number
            try {
                size_t pos;
                float val = std::stof(token, &pos);
                if (pos == token.size()) {
                    return val;
                }
            } catch (...) {}
            
            // Named sample - for now return 0, will be resolved at runtime
            // Common drum names mapped to indices
            static const std::unordered_map<std::string, int> drumMap = {
                {"bd", 0}, {"kick", 0}, {"k", 0},
                {"sd", 1}, {"snare", 1}, {"s", 1},
                {"hh", 2}, {"hihat", 2}, {"ch", 2}, {"oh", 3},
                {"clap", 4}, {"cp", 4},
                {"tom", 5}, {"lt", 5}, {"mt", 6}, {"ht", 7},
                {"cym", 8}, {"crash", 8}, {"ride", 9}
            };
            
            std::string lower = token;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            
            auto it = drumMap.find(lower);
            if (it != drumMap.end()) {
                return static_cast<float>(it->second);
            }
            
            // Unknown name - return 0 (first sample)
            return 0.0f;
        }
    };

    /**
     * External: A proxy for real-time external inputs (MIDI, OSC, Gamepad).
     * Ignores the cycle argument and pulls directly from an atomic float.
     */
    class External : public Pattern<float> {
    public:
        External(std::atomic<float>* ptr, const std::string& label = "") 
            : source(ptr), name(label) {}
        
        std::vector<Event<float>> query(double start, double end) override {
            // External inputs are continuous - return current value at start
            float val = source ? source->load(std::memory_order_acquire) : 0.0f;
            return {Event<float>(val, start, end - start, false)};
        }
        
        float eval(double cycle) override {
            return source ? source->load(std::memory_order_acquire) : 0.0f;
        }
        std::string getSignature() const override { return "ext:" + name; }
    private:
        std::atomic<float>* source;
        std::string name;
    };
}
