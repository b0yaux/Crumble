#pragma once
#include "../../core/Node.h"

// Screen output sink node
// Simply draws the input texture to the screen
class ScreenOutput : public Node {
public:
    ScreenOutput();
    
    void setup(float x, float y, float w, float h);
    void update(float dt) override;
    void draw();  // Call this in ofApp::draw()
    
    ofParameter<bool> enabled;
    ofParameter<float> x, y, width, height;
    
    // Input texture pulled from connected source during update()
    ofTexture* inputTexture = nullptr;
};
