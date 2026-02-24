#pragma once

#include "ofMain.h"
#include "core/Session.h"
#include "core/Config.h"

class GraphUI {
public:
    void setup();
    void draw(Session& session);
    
    void mousePressed(int x, int y, int button);
    void mouseDragged(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseScrolled(int x, int y, float scrollX, float scrollY);
    
    void setVisible(bool v) { visible = v; }
    bool isVisible() const { return visible; }
    
    void loadConfig(const std::string& path = "config.json");
    
private:
    bool visible = true;
    PhysicsConfig physicsConfig;
    
    glm::vec2 pan{0, 0};
    float zoom = 1.0f;
    glm::vec2 dragStart;
    glm::vec2 panStart;
    int draggedNode = -1;
    bool isDragging = false;
    glm::vec2 dragOffset;
    
    struct NodeViz {
        glm::vec2 pos;
        glm::vec2 vel;
        std::unordered_map<int, float> idealLengths; // Per-connection ideal lengths for "sticky" dragged positions
    };
    std::map<int, NodeViz> nodes;
    
    // Cached connections for use in mouseReleased (where session isn't available)
    std::vector<Connection> cachedConnections;
    
    glm::vec2 screenToWorld(int x, int y);
    void forceLayout(Session& session);
    void drawNode(Node* node, float x, float y);
};
