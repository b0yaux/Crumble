
#include "GraphUI.h"
#include <cmath>
#include <string>
#include <algorithm>
#include <map>
#include <set>
#include <queue>
#include <unordered_map>

void GraphUI::setup() {
    loadConfig();
}

void GraphUI::loadConfig(const std::string& path) {
    ConfigManager::get().load(path);
    physicsConfig = ConfigManager::get().getConfig().physics;
    ofLogNotice("GraphUI") << "Physics config loaded: damping=" << physicsConfig.damping 
                          << ", spring=" << physicsConfig.springStrength;
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
            isDragging = true;
            dragOffset = nv.pos - wp;
            nv.vel = {0, 0};
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
        nodes[draggedNode].vel = {0, 0};  // Keep velocity zeroed while dragging
        return;
    }
    
    pan.x = panStart.x + (x - dragStart.x);
    pan.y = panStart.y + (y - dragStart.y);
}

void GraphUI::mouseReleased(int x, int y, int button) {
    // Simply release the node - physics will naturally settle
    // No plasticity (idealLength updates) to avoid messy layouts after drag
    draggedNode = -1;
    isDragging = false;
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
    
    // Cache connections for use in mouseReleased (where session isn't available)
    cachedConnections = conns;
    
    // 1. Cleanup: Prune UI nodes that no longer exist in the core graph
    for (auto it = nodes.begin(); it != nodes.end(); ) {
        if (graphNodes.find(it->first) == graphNodes.end()) {
            it = nodes.erase(it);
        } else {
            ++it;
        }
    }
    
    // 2. Init: Island-aware depth-based positions for new nodes
    const float canvasW = 1200.0f;
    const float padding = 50.0f;
    const float layerHeight = 100.0f;
    
    // Build adjacency list for island detection (undirected)
    std::map<int, std::vector<int>> adj;
    for (auto& c : session.getGraph().getConnections()) {
        adj[c.fromNode].push_back(c.toNode);
        adj[c.toNode].push_back(c.fromNode);
    }
    
    // Identify Islands (Connected Components)
    std::set<int> visitedIsland;
    std::vector<std::vector<int>> islands;
    for (auto& n : graphNodes) {
        if (visitedIsland.find(n.first) == visitedIsland.end()) {
            std::vector<int> island;
            std::queue<int> q;
            q.push(n.first);
            visitedIsland.insert(n.first);
            while(!q.empty()) {
                int curr = q.front(); q.pop();
                island.push_back(curr);
                for(int neighbor : adj[curr]) {
                    if(visitedIsland.find(neighbor) == visitedIsland.end()) {
                        visitedIsland.insert(neighbor);
                        q.push(neighbor);
                    }
                }
            }
            islands.push_back(island);
        }
    }
    
    // Position each island in its own territory
    float sliceWidth = (canvasW - 2 * padding) / std::max(1, (int)islands.size());
    for (size_t islandIdx = 0; islandIdx < islands.size(); islandIdx++) {
        const auto& islandNodes = islands[islandIdx];
        float sliceX = padding + islandIdx * sliceWidth;
        
        // Compute depth within this island
        std::map<int, int> nodeDepth;
        std::map<int, std::vector<int>> nodesAtDepth;
        std::set<int> islandSet(islandNodes.begin(), islandNodes.end());
        
        // Find roots of this island (nodes with no inputs FROM WITHIN THIS ISLAND)
        std::queue<int> qDepth;
        for (int id : islandNodes) {
            bool hasIn = false;
            for (auto& c : session.getGraph().getConnections()) {
                if (c.toNode == id && islandSet.count(c.fromNode)) {
                    hasIn = true;
                    break;
                }
            }
            if (!hasIn) {
                nodeDepth[id] = 0;
                qDepth.push(id);
            }
        }
        
        // Fallback for islands that are cycles
        if (qDepth.empty() && !islandNodes.empty()) {
            nodeDepth[islandNodes[0]] = 0;
            qDepth.push(islandNodes[0]);
        }
        
        while (!qDepth.empty()) {
            int current = qDepth.front(); qDepth.pop();
            int d = nodeDepth[current];
            for (auto& c : session.getGraph().getConnections()) {
                if (c.fromNode == current && islandSet.count(c.toNode)) {
                    if (!nodeDepth.count(c.toNode)) {
                        nodeDepth[c.toNode] = d + 1;
                        qDepth.push(c.toNode);
                    }
                }
            }
        }
        
        // Group by depth
        for (int id : islandNodes) {
            int d = nodeDepth.count(id) ? nodeDepth[id] : 0;
            nodesAtDepth[d].push_back(id);
        }
        
        // Position new nodes in this island slice
        for (auto& [depth, nodeList] : nodesAtDepth) {
            float y = padding + depth * layerHeight;
            float xOffset = sliceWidth / (nodeList.size() + 1);
            for (size_t i = 0; i < nodeList.size(); i++) {
                int id = nodeList[i];
                if (!nodes.count(id)) {
                    float x = sliceX + (i + 1) * xOffset;
                    nodes[id] = {{x, y}, {0, 0}};
                }
            }
        }
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
        // Center to center
        ofDrawLine(from.pos.x + 40, from.pos.y + 12, to.pos.x + 40, to.pos.y + 12);
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
    
    // Check if physics is enabled
    if (!physicsConfig.enabled) return;
    
    // 1. Port crowding detection
    std::map<int, int> inDegree;
    std::map<int, int> outDegree;
    for (auto& c : conns) {
        outDegree[c.fromNode]++;
        inDegree[c.toNode]++;
    }
    
    // Physics Parameters: from Config
    const float damping = physicsConfig.damping;
    const float maxVel = physicsConfig.maxVelocity;
    const float baseSpringStrength = physicsConfig.springStrength;
    const float baseIdealLength = physicsConfig.idealEdgeLength;
    
    // 2. Apply spring forces
    // PLASTICITY: Only use stored idealLength if explicitly set by user drag.
    // Otherwise, use adaptive length for organic initial layout.
    for (auto& c : conns) {
        if (!nodes.count(c.fromNode) || !nodes.count(c.toNode)) continue;
        
        auto& from = nodes[c.fromNode];
        auto& to = nodes[c.toNode];
        
        int crowd = inDegree[c.toNode] + outDegree[c.fromNode];
        
        // ADAPTIVE RIGIDITY:
        // Crowded connections (clouds) are rigid and structured.
        // Rare connections (Mixer->Output) are stretchy and allow "long-range" drift.
        float strength = baseSpringStrength + std::min(crowd * 0.0015f, 0.04f);
        
        // Ideal length: rare connections stay close, crowded spread out
        // crowd=1: 60px, crowd=5: 90px, crowd=10: 120px, crowd=20: 170px
        float idealLength;
        if (crowd <= 1) {
            idealLength = baseIdealLength;
        } else if (crowd <= 5) {
            idealLength = baseIdealLength + (crowd - 1) * 7.5f;
        } else {
            idealLength = baseIdealLength + 30.0f + (crowd - 5) * 5.0f;
        }
        
        float dx = to.pos.x - from.pos.x;
        float dy = to.pos.y - from.pos.y;
        float dist = sqrtf(dx*dx + dy*dy);
        
        if (dist > 1.0f) {
            float displacement = dist - idealLength;
            float force = displacement * strength;
            
            float fx = (dx / dist) * force;
            float fy = (dy / dist) * force;
            
            from.vel.x += fx;
            from.vel.y += fy;
            to.vel.x -= fx;
            to.vel.y -= fy;
        }
    }
    
    // 3. Cleaner Repulsion: Inverse-distance based (Fast spreading)
    if (physicsConfig.repulsionEnabled) {
        const float repelRadius = physicsConfig.repulsionRadius;
        const float repelStrength = physicsConfig.repulsionStrength * 100.0f;
        
        for (auto& [idA, nodeA] : nodes) {
            for (auto& [idB, nodeB] : nodes) {
                if (idA >= idB) continue;
                
                float dx = (nodeB.pos.x + 40) - (nodeA.pos.x + 40);
                float dy = (nodeB.pos.y + 12) - (nodeA.pos.y + 12);
                float distSq = dx*dx + dy*dy;
                
                if (distSq < repelRadius * repelRadius && distSq > 1.0f) {
                    float dist = sqrtf(distSq);
                    
                    // Soft collision: quadratic force proportional to overlap
                    // Allows slight overlap to reduce jitter, but pushes apart
                    // Based on D3's forceCollide principle - soft constraint
                    const float minDist = 85.0f; // Node size + padding
                    float overlap = minDist - dist;
                    
                    if (overlap > 0) {
                        // Quadratic force - smooth, no explosion
                        float force = repelStrength * overlap * overlap * 0.01f;
                        
                        float fx = (dx / dist) * force;
                        float fy = (dy / dist) * force;
                        
                        nodeA.vel.x -= fx;
                        nodeA.vel.y -= fy;
                        nodeB.vel.x += fx;
                        nodeB.vel.y += fy;
                    }
                }
            }
        }
    }
    
    // 4. Update positions with "Drift"
    for (auto& [id, nv] : nodes) {
        // Dragged node position is fixed by mouse, but it still participates in physics
        // (pushes other nodes via springs/repulsion)
        if (id == draggedNode && isDragging) {
            nv.vel = {0, 0};
            continue;
        }

        // Apply damping
        nv.vel.x *= damping;
        nv.vel.y *= damping;
        
        // Clamp velocity
        nv.vel.x = std::max(-maxVel, std::min(maxVel, nv.vel.x));
        nv.vel.y = std::max(-maxVel, std::min(maxVel, nv.vel.y));
        
        // Update position
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
