#pragma once

#include "ofMain.h"
#include "core/Session.h"

class GraphUI {
public:
    void setup();
    void draw(Session& session);
    
    void mousePressed(int x, int y, int button);
    void mouseDragged(int x, int y, int button);
    void mouseScrolled(int x, int y, float scrollX, float scrollY);
    
    void setVisible(bool v) { visible = v; }
    bool isVisible() const { return visible; }
    
private:
    bool visible = true;
    
    glm::vec2 pan{0, 0};
    float zoom = 1.0f;
    glm::vec2 dragStart;
    glm::vec2 panStart;
    
    struct NodeViz {
        glm::vec2 pos;
        glm::vec2 vel;
    };
    std::map<int, NodeViz> nodes;
    
    glm::vec2 screenToWorld(int x, int y);
    void forceLayout(Session& session);
    void drawNode(Node* node, float x, float y);
};
