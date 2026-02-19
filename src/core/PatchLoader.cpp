#include "PatchLoader.h"
#include <ofFileUtils.h>
#include <ofLog.h>
#include <sstream>
#include <sys/stat.h>

PatchLoader::PatchLoader(Graph* graph)
    : graph(graph) {
}

static uint64_t getFileModificationTime(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

void PatchLoader::setPatchDirectory(const std::string& dir) {
    patchDir = dir;
    if (patchDir.empty()) return;
    if (patchDir.back() != '/' && patchDir.back() != '\\') {
        patchDir += "/";
    }
}

void PatchLoader::setPatchFile(const std::string& filename) {
    patchFilename = filename;
}

void PatchLoader::setNodeFactory(NodeFactory factory) {
    nodeFactory = std::move(factory);
}

void PatchLoader::setGraphBuilder(GraphBuilder builder) {
    graphBuilder = std::move(builder);
}

bool PatchLoader::load(const std::string& path) {
    currentPath = path;
    
    ofFile file(path);
    if (!file.exists()) {
        errorMessage = "File not found: " + path;
        ofLogError("PatchLoader") << errorMessage;
        return false;
    }
    
    lastModifiedTime = getFileModificationTime(path);
    
    std::stringstream buffer;
    buffer << file.readToBuffer();
    std::string jsonContent = buffer.str();
    
    return parseJson(jsonContent);
}

bool PatchLoader::reload() {
    ofFile file(currentPath);
    if (!file.exists()) {
        errorMessage = "File not found: " + currentPath;
        return false;
    }
    
    auto currentTime = getFileModificationTime(currentPath);
    if (currentTime == lastModifiedTime) {
        return true;
    }
    
    lastModifiedTime = currentTime;
    
    std::stringstream buffer;
    buffer << file.readToBuffer();
    std::string jsonContent = buffer.str();
    
    ofLogNotice("PatchLoader") << "Reloading patch: " << currentPath;
    return parseJson(jsonContent);
}

void PatchLoader::update() {
    if (!loaded || currentPath.empty()) return;
    reload();
}

bool PatchLoader::parseJson(const std::string& jsonContent) {
    ofJson json;
    try {
        json = ofJson::parse(jsonContent);
    } catch (const std::exception& e) {
        errorMessage = std::string("JSON parse error: ") + e.what();
        ofLogError("PatchLoader") << errorMessage;
        return false;
    }
    
    config = PatchConfig();
    
    if (json.contains("layers") && json["layers"].is_array()) {
        for (const auto& layerJson : json["layers"]) {
            LayerConfig layer;
            
            if (layerJson.contains("file") && layerJson["file"].is_string()) {
                layer.file = layerJson["file"];
            }
            
            if (layerJson.contains("opacity") && layerJson["opacity"].is_number()) {
                layer.opacity = layerJson["opacity"];
            }
            
            if (layerJson.contains("blend") && layerJson["blend"].is_string()) {
                layer.blend = layerJson["blend"];
            }
            
            if (layerJson.contains("active") && layerJson["active"].is_boolean()) {
                layer.active = layerJson["active"];
            }
            
            config.layers.push_back(layer);
        }
    }
    
    loaded = true;
    notifyGraphBuilder();
    return true;
}

void PatchLoader::notifyGraphBuilder() {
    if (graphBuilder) {
        graphBuilder(config);
    }
}
