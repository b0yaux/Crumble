#pragma once
#include "ofMain.h"

struct PhysicsConfig {
    bool enabled = true;
    float damping = 0.92f;
    float maxVelocity = 2.0f;
    float springStrength = 0.008f;
    float idealEdgeLength = 100.0f;
    bool repulsionEnabled = false;
    float repulsionStrength = 0.002f;
    float repulsionRadius = 100.0f;
};

struct LayoutConfig {
    std::string spawnPosition = "topological";
};

struct Config {
    PhysicsConfig physics;
    LayoutConfig layout;
    std::string entryScript;
    std::vector<std::string> searchPaths;
    std::string configPath = "config.json";
};

class ConfigManager {
public:
    static ConfigManager& get() {
        static ConfigManager instance;
        return instance;
    }

    bool load(const std::string& path = "config.json");
    std::string resolvePath(const std::string& path) const;
    const Config& getConfig() const { return config; }

private:
    ConfigManager() = default;
    Config config;
};
