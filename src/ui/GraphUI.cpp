#include "GraphUI.h"
#include <cmath>

void GraphUI::setup() {
}

glm::vec2 GraphUI::screenToWorld(int x, int y) {
    return glm::vec2((x - pan.x) / zoom, (y - pan.y) / zoom);
}

void GraphUI::mousePressed(int x, int y, int button) {
    if (!visible) return;
    dragStart = glm::vec2(x, y);
    panStart = pan;
}

void GraphUI::mouseDragged(int x, int y, int button) {
    if (!visible) return;
    pan.x = panStart.x + (x - dragStart.x);
    pan.y = panStart.y + (y - dragStart.y);
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
    
    for (auto& n : graphNodes) {
        if (!n || nodes.count(n->nodeIndex)) continue;
        int row = nodes.size() / 6;
        int col = nodes.size() % 6;
        nodes[n->nodeIndex] = {{100.0f + col * 120.0f, 100.0f + row * 80.0f}, {0, 0}};
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
        if (!n || !nodes.count(n->nodeIndex)) continue;
        drawNode(n.get(), nodes[n->nodeIndex].pos.x, nodes[n->nodeIndex].pos.y);
    }
    
    ofPopMatrix();
}

void GraphUI::forceLayout(Session& session) {
    auto& graphNodes = session.getGraph().getNodes();
    auto& conns = session.getGraph().getConnections();
    
    if (nodes.empty()) return;
    
    const float spacing = 100.0f;
    const float damping = 0.98f;
    const float maxVel = 5.0f;
    
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
    
    for (auto& [idA, nodeA] : nodes) {
        for (auto& [idB, nodeB] : nodes) {
            if (idA >= idB) continue;
            float dx = nodeB.pos.x - nodeA.pos.x;
            float dy = nodeB.pos.y - nodeA.pos.y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < spacing && dist > 1.0f) {
                float force = (spacing - dist) * 0.01f;
                float fx = (dx / dist) * force;
                float fy = (dy / dist) * force;
                nodeA.vel.x -= fx;
                nodeA.vel.y -= fy;
                nodeB.vel.x += fx;
                nodeB.vel.y += fy;
            }
        }
    }
    
    for (auto& c : conns) {
        if (!nodes.count(c.fromNode) || !nodes.count(c.toNode)) continue;
        auto& from = nodes[c.fromNode];
        auto& to = nodes[c.toNode];
        float dx = to.pos.x - from.pos.x;
        float dy = to.pos.y - from.pos.y;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist > 1.0f) {
            float force = (dist - spacing) * 0.005f;
            float fx = (dx / dist) * force;
            float fy = (dy / dist) * force;
            from.vel.x += fx;
            from.vel.y += fy;
            to.vel.x -= fx;
            to.vel.y -= fy;
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
    ofDrawBitmapString(node->name, x + 4, y + 16);
}
