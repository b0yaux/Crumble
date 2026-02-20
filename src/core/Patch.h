#pragma once
#include "Graph.h"

// Generic patch — owns a node graph, provides typed node access.
// No knowledge of specific node types (VideoMixer, etc.)
class Patch {
public:
    Patch() = default;

    // Lifecycle
    void update(float dt);

    // Node management — generic, works with any registered type
    Node* addNode(const std::string& type, const std::string& name = "");
    void removeNode(int index);
    void connect(int from, int to, int fromOut = 0, int toIn = 0);
    void disconnect(int toNode, int toIn = 0);
    void removeInput(int toNode, int toIn);

    // Node access — by index or name
    Node* getNode(int index);
    Node* getNode(const std::string& name);
    int getNodeCount() const;

    // Typed node access — caller supplies the type knowledge
    template<typename T>
    T* getNodeAs(int index) {
        return dynamic_cast<T*>(getNode(index));
    }
    template<typename T>
    T* getNodeAs(const std::string& name) {
        return dynamic_cast<T*>(getNode(name));
    }
    template<typename T>
    T* findFirstNodeOfType(const std::string& typeName) {
        for (const auto& node : graph.getNodes()) {
            if (node->type == typeName) {
                return dynamic_cast<T*>(node.get());
            }
        }
        return nullptr;
    }

    // Type registration — delegate to Graph factory
    void registerNodeType(const std::string& type, Graph::NodeCreator creator);

    // Persistence
    bool save(const std::string& path);
    bool load(const std::string& path);

    // Graph access — for commands, scripting, advanced use
    Graph& getGraph();
    const Graph& getGraph() const;

private:
    Graph graph;
};
