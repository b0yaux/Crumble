#pragma once
#include "ofMain.h"

struct PhysicsConfig {
    bool enabled = true;
    float damping = 0.92f;
    float maxVelocity = 2.0f;
    float springStrength = 0.008f;
    float idealEdgeLength = 100.0f;
    float centerGravity = 0.0005f;
    bool repulsionEnabled = false;
    float repulsionStrength = 0.002f;
    float repulsionRadius = 100.0f;
};

struct LayoutConfig {
    std::string initialPositioning = "topological";
};

struct Config {
    PhysicsConfig physics;
    LayoutConfig layout;
    std::vector<std::string> entryScripts;
    std::string defaultLuaPath = "scripts/main.lua";
    std::string defaultJsonPath = "scripts/main.json";
};

class ConfigManager {
public:
    static ConfigManager& get() {
        static ConfigManager instance;
        return instance;
    }

    bool load(const std::string& path = "config.json");
    const Config& getConfig() const { return config; }

private:
    ConfigManager() = default;
    Config config;
};
