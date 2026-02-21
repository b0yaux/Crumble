
#include "GraphUI.h"
#include <cmath>
#include <string>

void GraphUI::setup() {
}

glm::vec2 GraphUI::screenToWorld(int x, int y) {
    return glm::vec2((x - pan.x) / zoom, (y - pan.y) / zoom);
}

void GraphUI::mousePressed(int x, int y, int button) {
    if (!visible) return;
    
    glm::vec2 wp = screenToWorld(x, y);
    for (auto& [id, nv] : nodes) {
        if (wp.x >= nv.pos.x && wp.x <= nv.pos.x + 80 &&
            wp.y >= nv.pos.y && wp.y <= nv.pos.y + 24) {
            draggedNode = id;
            dragOffset = nv.pos - wp;
            return;
        }
    }
    
    dragStart = glm::vec2(x, y);
    panStart = pan;
}

void GraphUI::mouseDragged(int x, int y, int button) {
    if (!visible) return;
    
    if (draggedNode >= 0 && nodes.count(draggedNode)) {
        glm::vec2 wp = screenToWorld(x, y);
        nodes[draggedNode].pos = wp + dragOffset;
        nodes[draggedNode].vel = {0, 0};
        return;
    }
    
    pan.x = panStart.x + (x - dragStart.x);
    pan.y = panStart.y + (y - dragStart.y);
}

void GraphUI::mouseReleased(int x, int y, int button) {
    draggedNode = -1;
}

void GraphUI::mouseScrolled(int x, int y, float scrollX, float scrollY) {
    if (!visible) return;
    glm::vec2 wp = screenToWorld(x, y);
    zoom *= powf(1.1f, scrollY);
    zoom = glm::clamp(zoom, 0.2f, 3.0f);
    pan.x = x - wp.x * zoom;
    pan.y = y - wp.y * zoom;
}

void GraphUI::draw(Session& session) {
    if (!visible) return;
    
    auto& graphNodes = session.getGraph().getNodes();
    auto& conns = session.getGraph().getConnections();
    
    // Init: random positions with min distance from center (avoid center)
    // Sources spawn farther from center, outputs closer
    const float canvasW = 1200.0f;
    const float canvasH = 900.0f;
    for (auto& n : graphNodes) {
        if (!n.second || nodes.count(n.first)) continue;
        
        std::string t = n.second->type;
        bool isSource = (t.find("Source") != std::string::npos);
        float minDist = isSource ? 300.0f : 150.0f;
        
        float x, y;
        int attempts = 0;
        do {
            x = ofRandom(50.0f, canvasW - 50.0f);
            y = ofRandom(50.0f, canvasH - 50.0f);
            attempts++;
        } while (ofDist(x, y, canvasW/2, canvasH/2) < minDist && attempts < 50);
        
        nodes[n.first] = {{x, y}, {0, 0}};
    }
    
    forceLayout(session);
    
    ofPushMatrix();
    ofTranslate(pan.x, pan.y);
    ofScale(zoom, zoom);
    
    ofSetLineWidth(1);
    ofSetColor(60);
    for (auto& c : conns) {
        if (!nodes.count(c.fromNode) || !nodes.count(c.toNode)) continue;
        auto& from = nodes[c.fromNode];
        auto& to = nodes[c.toNode];
        ofDrawLine(from.pos.x + 80, from.pos.y + 12, to.pos.x, to.pos.y + 12);
    }
    
    for (auto& n : graphNodes) {
        if (!n.second || !nodes.count(n.first)) continue;
        drawNode(n.second.get(), nodes[n.first].pos.x, nodes[n.first].pos.y);
    }
    
    ofPopMatrix();
}

void GraphUI::forceLayout(Session& session) {
    auto& conns = session.getGraph().getConnections();
    
    if (nodes.empty()) return;
    
    // Physics: higher = more damping (slower movement), 0.9-0.99
    const float damping = 0.98f;
    // Physics: max pixels per frame, 0.5-5.0
    const float maxVel = 1.5f;
    
    std::map<int, int> outDegree;
    for (auto& c : conns) {
        outDegree[c.fromNode]++;
    }
    
    for (auto& [id, nv] : nodes) {
        if (!std::isfinite(nv.pos.x) || !std::isfinite(nv.pos.y)) {
            nv.pos = {200.0f, 200.0f};
            nv.vel = {0.0f, 0.0f};
        }
        nv.vel.x *= damping;
        nv.vel.y *= damping;
        
        if (nv.vel.x > maxVel) nv.vel.x = maxVel;
        if (nv.vel.x < -maxVel) nv.vel.x = -maxVel;
        if (nv.vel.y > maxVel) nv.vel.y = maxVel;
        if (nv.vel.y < -maxVel) nv.vel.y = -maxVel;
    }
    
    // Physics: base distance between nodes, 50-200
    const float spacing = 100.0f;
    
    // Physics: repulsion when closer than spacing, 0.0005-0.005
    for (auto& [idA, nodeA] : nodes) {
        for (auto& [idB, nodeB] : nodes) {
            if (idA >= idB) continue;
            float dx = nodeB.pos.x - nodeA.pos.x;
            float dy = nodeB.pos.y - nodeA.pos.y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < spacing && dist > 1.0f) {
                float force = (spacing - dist) * 0.002f;
                float fx = (dx / dist) * force;
                float fy = (dy / dist) * force;
                nodeA.vel.x -= fx;
                nodeA.vel.y -= fy;
                nodeB.vel.x += fx;
                nodeB.vel.y += fy;
            }
        }
    }
    
    // Physics: spring attraction, 0.001-0.01
    // Tweak: targetDegree controls uniqueness - fewer connections to target = tighter bond
    std::map<int, int> inDegree;
    for (auto& c : conns) {
        inDegree[c.toNode]++;
    }
    
    for (auto& c : conns) {
        if (!nodes.count(c.fromNode) || !nodes.count(c.toNode)) continue;
        auto& from = nodes[c.fromNode];
        auto& to = nodes[c.toNode];
        float dx = to.pos.x - from.pos.x;
        float dy = to.pos.y - from.pos.y;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist > 1.0f) {
            float targetDegree = (float)inDegree.count(c.toNode) ? inDegree[c.toNode] : 1;
            float targetDist = spacing * (0.2f + targetDegree * 0.04f);
            if (targetDist < spacing * 0.5f) targetDist = spacing * 0.5f;
            float force = (dist - targetDist) * 0.002f;
            float fx = (dx / dist) * force;
            float fy = (dy / dist) * force;
            from.vel.x += fx;
            from.vel.y += fy;
            to.vel.x -= fx;
            to.vel.y -= fy;
        }
    }
    
    // Physics: gravity toward center for output nodes only
    // Tweak: centerX, centerY, gravity strength (0.001-0.02)
    const float centerX = 400.0f;
    const float centerY = 300.0f;
    const float outputGravity = 0.002f;
    auto& graphNodes = session.getGraph().getNodes();
    for (auto& n : graphNodes) {
        if (!n.second || !nodes.count(n.first)) continue;
        if (n.second->type.find("Output") != std::string::npos) {
            auto& nv = nodes[n.first];
            nv.vel.x += (centerX - nv.pos.x) * outputGravity;
            nv.vel.y += (centerY - nv.pos.y) * outputGravity;
        }
    }
    
    for (auto& [id, nv] : nodes) {
        nv.pos.x += nv.vel.x;
        nv.pos.y += nv.vel.y;
    }
}

void GraphUI::drawNode(Node* node, float x, float y) {
    ofSetColor(30, 30, 30, 150);
    ofFill();
    ofDrawRectangle(x, y, 80, 24);
    
    ofNoFill();
    ofSetColor(200);
    ofSetLineWidth(1);
    ofDrawRectangle(x, y, 80, 24);
    
    ofFill();
    ofSetColor(200);
    
    std::string displayName = node->name;
    const float charWidth = 8.0f;
    int maxChars = static_cast<int>(80.0f / (charWidth / zoom));
    maxChars = std::max(maxChars, 4);
    if ((int)displayName.size() > maxChars) {
        displayName = displayName.substr(0, maxChars - 1) + "~";
    }
    ofDrawBitmapString(displayName, x + 4, y + 16);
}
