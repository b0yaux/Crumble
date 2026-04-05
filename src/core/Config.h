#pragma once
#include "ofMain.h"

// Session configuration, loaded from config.json at startup and on hot-reload.

struct PhysicsConfig {
    bool enabled = true;
    bool randomSpawn = false;
    float damping = 0.92f;
    float maxVelocity = 15.0f;
    float springStrength = 0.008f;
    float idealEdgeLength = 100.0f;
    float connectionSpacing = 20.0f;
    bool repulsionEnabled = true;
    float repulsionStrength = 0.002f;
    float repulsionRadius = 100.0f;
};

struct GeometryConfig {
    float nodeWidth = 80.0f;
    float nodeHeight = 24.0f;
    float padding = 50.0f;
    float recursiveScale = 10.0f;
    float portalRadius = 6.0f;
};

struct ThemeConfig {
    ofColor nodeBackground = ofColor(30, 30, 30, 150);
    ofColor nodeBorder = ofColor(200, 200, 200, 255);
    ofColor nodeText = ofColor(200, 200, 200, 255);
    ofColor connectionEdge = ofColor(60, 60, 60, 255);
    ofColor portalRing = ofColor(80, 80, 80, 200);
};

struct Config {
    PhysicsConfig physics;
    GeometryConfig geometry;
    ThemeConfig theme;
    std::string entryScript;
    std::vector<std::string> searchPaths;
    std::string configPath = "config.json";
};

// Singleton config loader. Owns the parsed Config struct.

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
