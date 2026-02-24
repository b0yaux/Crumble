#include "Config.h"

bool ConfigManager::load(const std::string& path) {
    std::string absPath = ofToDataPath(path);
    std::ifstream file(absPath);
    
    if (!file.is_open()) {
        ofLogWarning("Config") << "Config file not found, using defaults: " << absPath;
        return false;
    }
    
    try {
        ofJson j = ofJson::parse(file);
        
        if (j.contains("graph") && j["graph"].contains("physics")) {
            auto& p = j["graph"]["physics"];
            config.physics.enabled = p.value("enabled", config.physics.enabled);
            config.physics.damping = p.value("damping", config.physics.damping);
            config.physics.maxVelocity = p.value("maxVelocity", config.physics.maxVelocity);
            config.physics.springStrength = p.value("springStrength", config.physics.springStrength);
            config.physics.idealEdgeLength = p.value("idealEdgeLength", config.physics.idealEdgeLength);
            config.physics.centerGravity = p.value("centerGravity", config.physics.centerGravity);
            config.physics.repulsionEnabled = p.value("repulsionEnabled", config.physics.repulsionEnabled);
            config.physics.repulsionStrength = p.value("repulsionStrength", config.physics.repulsionStrength);
            config.physics.repulsionRadius = p.value("repulsionRadius", config.physics.repulsionRadius);
        }
        
        if (j.contains("graph") && j["graph"].contains("layout")) {
            auto& l = j["graph"]["layout"];
            config.layout.initialPositioning = l.value("initialPositioning", config.layout.initialPositioning);
        }
        
        if (j.contains("entryScript")) {
            config.entryScript = j["entryScript"].get<std::string>();
        }
        
        if (j.contains("paths")) {
            config.defaultLuaPath = j["paths"].value("lua", config.defaultLuaPath);
            config.defaultJsonPath = j["paths"].value("json", config.defaultJsonPath);
        }
        
        ofLogNotice("Config") << "Loaded config from: " << absPath;
        return true;
        
    } catch (const std::exception& e) {
        ofLogError("Config") << "Error loading config: " << e.what();
        return false;
    }
}
