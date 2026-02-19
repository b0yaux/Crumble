#pragma once
#include "Graph.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

struct LayerConfig {
    std::string file;
    float opacity = 1.0f;
    std::string blend = "ALPHA";
    bool active = true;
};

struct PatchConfig {
    std::vector<LayerConfig> layers;
};

class PatchLoader {
public:
    using NodeFactory = std::function<std::unique_ptr<Node>(const std::string& type, const std::string& name)>;
    using GraphBuilder = std::function<void(const PatchConfig& config)>;
    
    PatchLoader(Graph* graph);
    
    void setPatchDirectory(const std::string& dir);
    void setPatchFile(const std::string& filename);
    
    void setNodeFactory(NodeFactory factory);
    void setGraphBuilder(GraphBuilder builder);
    
    bool load(const std::string& path);
    bool reload();
    
    void update();
    
    bool isLoaded() const { return loaded; }
    const std::string& getCurrentPath() const { return currentPath; }
    const std::string& getError() const { return errorMessage; }
    const PatchConfig& getConfig() const { return config; }
    
private:
    Graph* graph = nullptr;
    NodeFactory nodeFactory;
    GraphBuilder graphBuilder;
    
    PatchConfig config;
    
    std::string patchDir;
    std::string patchFilename;
    std::string currentPath;
    std::string errorMessage;
    bool loaded = false;
    
    uint64_t lastModifiedTime = 0;
    
    bool parseJson(const std::string& jsonContent);
    void notifyGraphBuilder();
};
