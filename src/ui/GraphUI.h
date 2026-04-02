#pragma once

#include "ofMain.h"
#include "core/Session.h"
#include "core/Config.h"

class GraphUI {
public:
    void setup();
    void draw(Session& session);
    
    void mouseMoved(int x, int y);
    void mousePressed(int x, int y, int button);
    void mouseDragged(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseScrolled(int x, int y, float scrollX, float scrollY);
    
    void setVisible(bool v) { visible = v; }
    bool isVisible() const { return visible; }
    
    void loadConfig(const std::string& path = "config.json");
    
private:
    struct NodeViz {
        glm::vec2 pos;
        glm::vec2 vel;
        std::unordered_map<int, float> idealLengths;
    };

    struct LevelViz {
        std::map<int, NodeViz> nodes;
        std::map<int, std::unique_ptr<LevelViz>> children;
    };

    bool visible = true;
    PhysicsConfig physicsConfig;
    GeometryConfig geometryConfig;
    ThemeConfig themeConfig;
    
    glm::vec2 pan{0, 0};
    float zoom = 1.0f;
    glm::vec2 dragStart;
    glm::vec2 panStart;
    
    // Drag state
    int draggedNode = -1;
    LevelViz* draggedLevel = nullptr; 
    bool isDragging = false;
    
    // Hover Navigation State
    bool isHoverPanning = false;
    bool isHoverZooming = false;
    float zoomStart = 1.0f;
    glm::vec2 zoomMouseStart;
    glm::vec2 zoomWorldStart;
    
    glm::vec2 dragOffset;
    
    LevelViz rootLevel;
    
    glm::vec2 screenToWorld(int x, int y);
    
    void initLayout(Graph& graph, std::map<int, NodeViz>& vizNodes,
                    float canvasW, float canvasH);
    void forceLayout(Graph& graph, const std::vector<Connection>& conns,
                     LevelViz& level, float canvasW, float canvasH);
    void drawGraph(Graph& graph, LevelViz& level, float effectiveZoom,
                   float canvasW, float canvasH);
    void drawNode(Node* node, float x, float y, float effectiveZoom, bool hasConnections);
    
    bool hitTest(LevelViz& level, glm::vec2 localMouse,
                 int& foundId, LevelViz*& foundLevel, glm::vec2& foundOffset);
};
