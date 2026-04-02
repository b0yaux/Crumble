
#include "GraphUI.h"
#include <cmath>
#include <string>
#include <algorithm>
#include <map>
#include <set>
#include <queue>
#include <unordered_map>
#include <functional>

void GraphUI::setup() {
    loadConfig();
}

void GraphUI::loadConfig(const std::string& path) {
    ConfigManager::get().load(path);
    physicsConfig = ConfigManager::get().getConfig().physics;
    geometryConfig = ConfigManager::get().getConfig().geometry;
    themeConfig = ConfigManager::get().getConfig().theme;
    ofLogNotice("GraphUI") << "Config loaded: damping=" << physicsConfig.damping
                           << ", spring=" << physicsConfig.springStrength
                           << ", nodeSize=" << geometryConfig.nodeWidth << "x" << geometryConfig.nodeHeight
                           << ", recursiveScale=" << geometryConfig.recursiveScale;
}

glm::vec2 GraphUI::screenToWorld(int x, int y) {
    return glm::vec2((x - pan.x) / zoom, (y - pan.y) / zoom);
}

void GraphUI::mouseMoved(int x, int y) {
    if (!visible) return;
    
    // Alt + Move Mouse = Smooth Zoom (Focus on cursor)
    if (ofGetKeyPressed(OF_KEY_ALT)) {
        isHoverPanning = false;
        if (!isHoverZooming) {
            zoomMouseStart = glm::vec2(x, y);
            zoomWorldStart = screenToWorld(x, y);
            zoomStart = zoom;
            isHoverZooming = true;
        }
        float dy = y - zoomMouseStart.y;
        zoom = zoomStart * expf(-dy * 0.01f);
        zoom = glm::clamp(zoom, 0.0001f, 10000.0f);
        pan.x = zoomMouseStart.x - zoomWorldStart.x * zoom;
        pan.y = zoomMouseStart.y - zoomWorldStart.y * zoom;
    } 
    // Shift + Move Mouse = Pan (without clicking)
    else if (ofGetKeyPressed(OF_KEY_SHIFT)) {
        isHoverZooming = false;
        if (!isHoverPanning) {
            dragStart = glm::vec2(x, y);
            panStart = pan;
            isHoverPanning = true;
        }
        pan.x = panStart.x + (x - dragStart.x);
        pan.y = panStart.y + (y - dragStart.y);
    } else {
        isHoverPanning = false;
        isHoverZooming = false;
    }
}

void GraphUI::mousePressed(int x, int y, int button) {
    if (!visible) return;
    glm::vec2 wp = screenToWorld(x, y);
    
    // Left-Click (0) WITHOUT Shift: Try to grab a node
    if (button == 0 && !ofGetKeyPressed(OF_KEY_SHIFT)) {
        if (hitTest(rootLevel, wp, draggedNode, draggedLevel, dragOffset)) {
            isDragging = true;
            draggedLevel->nodes[draggedNode].vel = {0, 0};
            return;
        }
    }
    
    // Right-Click (2), Middle-Click (1), or missed Left-Click: Pan the camera
    dragStart = glm::vec2(x, y);
    panStart = pan;
}

bool GraphUI::hitTest(LevelViz& level, glm::vec2 localMouse,
                      int& foundId, LevelViz*& foundLevel, glm::vec2& foundOffset) {
    float nw = geometryConfig.nodeWidth;
    float nh = geometryConfig.nodeHeight;
    float localScale = 1.0f / geometryConfig.recursiveScale;

    for (auto it = level.children.rbegin(); it != level.children.rend(); ++it) {
        if (!level.nodes.count(it->first)) continue;
        glm::vec2 nodePos = level.nodes[it->first].pos;
        glm::vec2 childMouse = (localMouse - nodePos) / localScale;
        if (hitTest(*it->second, childMouse, foundId, foundLevel, foundOffset)) return true;
    }

    for (auto it = level.nodes.rbegin(); it != level.nodes.rend(); ++it) {
        if (localMouse.x >= it->second.pos.x && localMouse.x <= it->second.pos.x + nw &&
            localMouse.y >= it->second.pos.y && localMouse.y <= it->second.pos.y + nh) {
            foundId = it->first;
            foundLevel = &level;
            foundOffset = it->second.pos - localMouse;
            return true;
        }
    }

    return false;
}

void GraphUI::mouseDragged(int x, int y, int button) {
    if (!visible) return;
    
    if (button == 0 && isDragging && draggedLevel && draggedNode >= 0) {
        glm::vec2 wp = screenToWorld(x, y);
        std::function<bool(LevelViz&, LevelViz*, glm::vec2, glm::vec2&)> toLocal = 
            [&](LevelViz& current, LevelViz* target, glm::vec2 p, glm::vec2& outPos) -> bool {
                if (&current == target) { outPos = p; return true; }
                for (auto& [id, child] : current.children) {
                    if (!current.nodes.count(id)) continue;
                    glm::vec2 childP = (p - current.nodes[id].pos) / (1.0f / geometryConfig.recursiveScale);
                    if (toLocal(*child, target, childP, outPos)) return true;
                }
                return false; 
            };

        glm::vec2 lp;
        if (toLocal(rootLevel, draggedLevel, wp, lp)) {
            draggedLevel->nodes[draggedNode].pos = lp + dragOffset;
            draggedLevel->nodes[draggedNode].vel = {0, 0};
        }
        return;
    }
    
    pan.x = panStart.x + (x - dragStart.x);
    pan.y = panStart.y + (y - dragStart.y);
}

void GraphUI::mouseReleased(int x, int y, int button) {
    draggedNode = -1;
    draggedLevel = nullptr;
    isDragging = false;
}

void GraphUI::mouseScrolled(int x, int y, float scrollX, float scrollY) {
    if (!visible) return;
    glm::vec2 wp = screenToWorld(x, y);
    zoom *= powf(1.1f, scrollY);
    zoom = glm::clamp(zoom, 0.0001f, 10000.0f);
    pan.x = x - wp.x * zoom;
    pan.y = y - wp.y * zoom;
}

void GraphUI::draw(Session& session) {
    if (!visible) return;
    
    ofPushMatrix();
    ofTranslate(pan.x, pan.y);
    ofScale(zoom, zoom);
    drawGraph(session.getGraph(), rootLevel, zoom, ofGetWidth(), ofGetHeight());
    ofPopMatrix();
}

void GraphUI::drawGraph(Graph& graph, LevelViz& level, float effectiveZoom,
                        float canvasW, float canvasH) {
    float nw = geometryConfig.nodeWidth;
    float nh = geometryConfig.nodeHeight;
    float halfW = nw * 0.5f;
    float halfH = nh * 0.5f;
    float localScale = 1.0f / geometryConfig.recursiveScale;
    
    initLayout(graph, level.nodes, canvasW, canvasH);
    forceLayout(graph, graph.getConnections(), level, canvasW, canvasH);
    
    // Draw Connections
    auto& conns = graph.getConnections();
    ofSetLineWidth(1);
    ofSetColor(themeConfig.connectionEdge);
    for (auto& c : conns) {
        if (!level.nodes.count(c.fromNode) || !level.nodes.count(c.toNode)) continue;
        glm::vec2 fromPos = level.nodes[c.fromNode].pos + glm::vec2(halfW, halfH);
        glm::vec2 toPos   = level.nodes[c.toNode].pos + glm::vec2(halfW, halfH);
        ofDrawLine(fromPos.x, fromPos.y, toPos.x, toPos.y);
    }
    
    // Draw Portal Ring
    if (&level != &rootLevel) {
        glm::vec2 canvasCenter(canvasW * 0.5f, canvasH * 0.5f);
        ofSetColor(themeConfig.portalRing);
        ofNoFill();
        ofSetLineWidth(2);
        ofDrawCircle(canvasCenter.x, canvasCenter.y, geometryConfig.portalRadius);
        ofSetLineWidth(1);
        ofSetColor(themeConfig.connectionEdge);
        for (const auto& inlet : graph.getInlets()) {
            if (inlet.node && level.nodes.count(inlet.node->nodeId)) {
                glm::vec2 p = level.nodes[inlet.node->nodeId].pos + glm::vec2(halfW, halfH);
                ofDrawLine(canvasCenter.x, canvasCenter.y, p.x, p.y);
            }
        }
        for (const auto& outlet : graph.getOutlets()) {
            if (outlet.node && level.nodes.count(outlet.node->nodeId)) {
                glm::vec2 p = level.nodes[outlet.node->nodeId].pos + glm::vec2(halfW, halfH);
                ofDrawLine(p.x, p.y, canvasCenter.x, canvasCenter.y);
            }
        }
    }
    
    // Track connections per node for anchor drawing
    std::set<int> connectedNodes;
    for (auto& c : conns) { connectedNodes.insert(c.fromNode); connectedNodes.insert(c.toNode); }

    // Draw Nodes & Recurse
    auto& graphNodes = graph.getNodes();
    for (auto& n : graphNodes) {
        if (!n.second || !level.nodes.count(n.first)) continue;
        Node* node = n.second.get();
        float x = level.nodes[n.first].pos.x;
        float y = level.nodes[n.first].pos.y;
        
        drawNode(node, x, y, effectiveZoom, connectedNodes.count(n.first) > 0);
        
        if (node->type == "graph") {
            Graph* childGraph = dynamic_cast<Graph*>(node);
            if (childGraph && childGraph->getNodeCount() > 0) {
                auto it = level.children.find(n.first);
                if (it == level.children.end()) it = level.children.emplace(n.first, std::make_unique<LevelViz>()).first;
                
                float childCanvasW = nw * geometryConfig.recursiveScale;
                float childCanvasH = nh * geometryConfig.recursiveScale;
                
                ofPushMatrix();
                ofTranslate(x, y);
                ofScale(localScale, localScale);
                drawGraph(*childGraph, *it->second, effectiveZoom * localScale, childCanvasW, childCanvasH);
                ofPopMatrix();
            }
        }
    }
}

void GraphUI::initLayout(Graph& graph, std::map<int, NodeViz>& vizNodes,
                          float canvasW, float canvasH) {
    auto& graphNodes = graph.getNodes();
    for (auto it = vizNodes.begin(); it != vizNodes.end(); ) {
        if (graphNodes.find(it->first) == graphNodes.end()) it = vizNodes.erase(it); else ++it;
    }
    if (graphNodes.empty()) return;
    
    // Calculate Depth for Topological Seed
    std::map<int, int> inDegree;
    std::map<int, std::vector<int>> outgoing;
    for (auto& c : graph.getConnections()) { inDegree[c.toNode]++; outgoing[c.fromNode].push_back(c.toNode); }
    std::map<int, int> depth;
    std::queue<int> q;
    for (auto& n : graphNodes) { if (inDegree[n.first] == 0) { depth[n.first] = 0; q.push(n.first); } }
    if (q.empty() && !graphNodes.empty()) { depth[graphNodes.begin()->first] = 0; q.push(graphNodes.begin()->first); }
    int maxDepth = 0;
    while (!q.empty()) {
        int curr = q.front(); q.pop();
        maxDepth = std::max(maxDepth, depth[curr]);
        for (int child : outgoing[curr]) { if (!depth.count(child)) { depth[child] = depth[curr] + 1; q.push(child); } }
    }

    float nw = geometryConfig.nodeWidth, nh = geometryConfig.nodeHeight;
    float cx = canvasW * 0.5f - nw * 0.5f, cy = canvasH * 0.5f - nh * 0.5f;
    float seedSize = std::min(150.0f, (float)graphNodes.size() * 5.0f);
    
    std::map<int, int> nodesAtDepthCount, nodesAtDepthIndex;
    for (auto& n : graphNodes) { if (!vizNodes.count(n.first)) nodesAtDepthCount[depth.count(n.first) ? depth[n.first] : 0]++; }

    for (auto& n : graphNodes) {
        if (!vizNodes.count(n.first)) {
            int d = depth.count(n.first) ? depth[n.first] : 0;
            int idx = nodesAtDepthIndex[d]++, count = nodesAtDepthCount[d];
            float nDepth = maxDepth > 0 ? (float)d / maxDepth : 0.5f;
            float nWidth = count > 1 ? (float)idx / (count - 1) : 0.5f;
            vizNodes[n.first] = {{cx + (nWidth - 0.5f) * seedSize, cy + (nDepth - 0.5f) * seedSize}, {0, 0}};
        }
    }
}

void GraphUI::forceLayout(Graph& graph, const std::vector<Connection>& conns,
                           LevelViz& level, float canvasW, float canvasH) {
    auto& vizNodes = level.nodes;
    if (vizNodes.empty() || !physicsConfig.enabled) return;
    
    float nw = geometryConfig.nodeWidth, nh = geometryConfig.nodeHeight;
    float halfW = nw * 0.5f, halfH = nh * 0.5f;
    
    std::map<int, int> inDegree, outDegree;
    for (auto& c : conns) { outDegree[c.fromNode]++; inDegree[c.toNode]++; }
    
    const float damping = physicsConfig.damping, maxVel = physicsConfig.maxVelocity;
    const float strength = physicsConfig.springStrength, baseLen = physicsConfig.idealEdgeLength, bonus = physicsConfig.connectionSpacing;
    
    // 1. Spring Forces
    for (auto& c : conns) {
        if (!vizNodes.count(c.fromNode) || !vizNodes.count(c.toNode)) continue;
        auto &f = vizNodes[c.fromNode], &t = vizNodes[c.toNode];
        int extra = std::max(0, (inDegree[c.toNode] + outDegree[c.fromNode]) - 2);
        float ideal = baseLen + (extra * bonus);
        float dx = t.pos.x - f.pos.x, dy = t.pos.y - f.pos.y, dist = sqrtf(dx*dx + dy*dy);
        
        bool beingDragged = isDragging && &level == draggedLevel && (c.fromNode == draggedNode || c.toNode == draggedNode);
        
        if (dist > 1.0f) {
            float displacement = dist - ideal;
            if (displacement < 0.0f && beingDragged) displacement = 0.0f; // Yielding Springs
            float fv = displacement * strength;
            float mF = std::max(1, inDegree[c.fromNode] + outDegree[c.fromNode]), mT = std::max(1, inDegree[c.toNode] + outDegree[c.toNode]);
            f.vel += (glm::vec2(dx, dy) / dist) * fv * (mT / (mF + mT));
            t.vel -= (glm::vec2(dx, dy) / dist) * fv * (mF / (mF + mT));
        }
    }
    
    // 1b. Wormhole Gravity
    if (&level != &rootLevel) {
        glm::vec2 center(canvasW * 0.5f - halfW, canvasH * 0.5f - halfH);
        auto tether = [&](int id) {
            if (!vizNodes.count(id)) return;
            float dx = center.x - vizNodes[id].pos.x, dy = center.y - vizNodes[id].pos.y, dist = sqrtf(dx*dx + dy*dy);
            if (dist > baseLen * 0.8f) vizNodes[id].vel += (glm::vec2(dx, dy) / dist) * (dist - baseLen * 0.8f) * strength;
        };
        for (const auto& i : graph.getInlets()) if (i.node) tether(i.node->nodeId);
        for (const auto& o : graph.getOutlets()) if (o.node) tether(o.node->nodeId);
    }
    
    // 2. Social Repulsion
    if (physicsConfig.repulsionEnabled) {
        for (auto itA = vizNodes.begin(); itA != vizNodes.end(); ++itA) {
            for (auto itB = std::next(itA); itB != vizNodes.end(); ++itB) {
                float dx = itB->second.pos.x - itA->second.pos.x, dy = itB->second.pos.y - itA->second.pos.y, dist = sqrtf(dx*dx + dy*dy);
                if (dist < physicsConfig.repulsionRadius && dist > 0.1f) {
                    glm::vec2 f = (glm::vec2(dx, dy) / dist) * physicsConfig.repulsionStrength * (physicsConfig.repulsionRadius - dist);
                    itA->second.vel -= f; itB->second.vel += f;
                }
            }
        }
    }
    
    // 3. Integration & Bounds
    float padX = std::min(geometryConfig.padding, (canvasW - nw) * 0.49f), padY = std::min(geometryConfig.padding, (canvasH - nh) * 0.49f);
    for (auto& [id, nv] : vizNodes) {
        if (isDragging && &level == draggedLevel && id == draggedNode) {
            nv.vel = {0, 0};
            nv.pos.x = std::max(padX, std::min(canvasW - nw - padX, nv.pos.x));
            nv.pos.y = std::max(padY, std::min(canvasH - nh - padY, nv.pos.y));
            continue;
        }
        nv.vel = glm::clamp(nv.vel * damping, -maxVel, maxVel);
        nv.pos += nv.vel;
        if (nv.pos.x < padX) { nv.pos.x = padX; nv.vel.x *= -1; } if (nv.pos.y < padY) { nv.pos.y = padY; nv.vel.y *= -1; }
        if (nv.pos.x > canvasW - nw - padX) { nv.pos.x = canvasW - nw - padX; nv.vel.x *= -1; } if (nv.pos.y > canvasH - nh - padY) { nv.pos.y = canvasH - nh - padY; nv.vel.y *= -1; }
    }

    // 4. Rigid Body Geometry Pass (Solid AABB Collisions)
    for (int iter = 0; iter < 2; iter++) {
        for (auto itA = vizNodes.begin(); itA != vizNodes.end(); ++itA) {
            for (auto itB = std::next(itA); itB != vizNodes.end(); ++itB) {
                float dx = itB->second.pos.x - itA->second.pos.x, dy = itB->second.pos.y - itA->second.pos.y;
                float ox = nw - std::abs(dx), oy = nh - std::abs(dy);
                if (ox > 0 && oy > 0) {
                    float wA = (isDragging && &level == draggedLevel && itA->first == draggedNode) ? 0.0f : ((isDragging && &level == draggedLevel && itB->first == draggedNode) ? 1.0f : 0.5f);
                    float wB = 1.0f - wA;
                    if (ox < oy) { float s = ox * (dx > 0 ? 1 : -1); itA->second.pos.x -= s * wA; itB->second.pos.x += s * wB; itA->second.vel.x *= (1.0f-wA); itB->second.vel.x *= (1.0f-wB); }
                    else { float s = oy * (dy > 0 ? 1 : -1); itA->second.pos.y -= s * wA; itB->second.pos.y += s * wB; itA->second.vel.y *= (1.0f-wA); itB->second.vel.y *= (1.0f-wB); }
                }
            }
        }
    }
}

void GraphUI::drawNode(Node* node, float x, float y, float effectiveZoom, bool hasConnections) {
    float nw = geometryConfig.nodeWidth, nh = geometryConfig.nodeHeight;
    ofFill(); ofSetColor(themeConfig.nodeBackground); ofDrawRectangle(x, y, nw, nh);
    ofNoFill(); ofSetColor(themeConfig.nodeBorder); ofSetLineWidth(1); ofDrawRectangle(x, y, nw, nh);
    ofFill(); ofSetColor(themeConfig.nodeText);
    std::string name = node->name; int maxC = std::max(4, (int)(nw / (8.0f / effectiveZoom)));
    if ((int)name.size() > maxC) name = name.substr(0, maxC - 1) + "~";
    ofDrawBitmapString(name, x + (4.0f / effectiveZoom), y + (13.0f / effectiveZoom));
    if (hasConnections) { ofSetColor(themeConfig.connectionEdge); ofFill(); ofDrawCircle(x + nw*0.5f, y + nh*0.5f, 1.5f / effectiveZoom); }
}
