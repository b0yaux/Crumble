#include "Config.h"
#include "AssetRegistry.h"

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
            config.physics.repulsionEnabled = p.value("repulsionEnabled", config.physics.repulsionEnabled);
            config.physics.repulsionStrength = p.value("repulsionStrength", config.physics.repulsionStrength);
            config.physics.repulsionRadius = p.value("repulsionRadius", config.physics.repulsionRadius);
        }
        
        if (j.contains("graph") && j["graph"].contains("layout")) {
            auto& l = j["graph"]["layout"];
            config.layout.spawnPosition = l.value("spawnPosition", config.layout.spawnPosition);
        }
        
        if (j.contains("entryScript")) {
            config.entryScript = j["entryScript"].get<std::string>();
        }
        
        if (j.contains("searchPaths")) {
            config.searchPaths.clear();
            for (auto& p : j["searchPaths"]) {
                config.searchPaths.push_back(p.get<std::string>());
            }
            // Trigger the AssetRegistry scan
            AssetRegistry::get().scan(config.searchPaths);
        }
        
        ofLogNotice("Config") << "Loaded config from: " << absPath;
        return true;
        
    } catch (const std::exception& e) {
        ofLogError("Config") << "Error loading config: " << e.what();
        return false;
    }
}

std::string ConfigManager::resolvePath(const std::string& path) const {
    // 1. If absolute, use as-is
    if (!path.empty() && ofFilePath::isAbsolute(path)) {
        if (ofFile::doesFileExist(path)) return path;
    }

    // 2. Try relative to the current script directory (Implicit project)
    if (!path.empty() && !config.entryScript.empty()) {
        std::string scriptDir = ofFilePath::getEnclosingDirectory(ofToDataPath(config.entryScript));
        std::string projectPath = ofFilePath::join(scriptDir, path);
        if (ofFile::doesFileExist(projectPath)) return projectPath;
    }

    // 3. Try each configured search path
    for (const auto& searchPath : config.searchPaths) {
        // Simple tilde expansion for macOS/Linux
        std::string expanded = searchPath;
        if (!expanded.empty() && expanded[0] == '~') {
            const char* home = getenv("HOME");
            if (home) expanded = std::string(home) + expanded.substr(1);
        }
        
        std::string fullPath = ofFilePath::join(expanded, path);
        // Use doesFileExist which also handles directories in OF
        if (ofFile::doesFileExist(fullPath)) return fullPath;
    }

    // 4. Fallback: standard openFrameworks data path
    if (!path.empty()) {
        std::string dataPath = ofToDataPath(path);
        if (ofFile::doesFileExist(dataPath)) return dataPath;
    }

    // Return original if nothing found (honest failure)
    return path;
}
