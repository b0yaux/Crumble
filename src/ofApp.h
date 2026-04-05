#pragma once
#include "ofMain.h"
#include "core/Session.h"
#include "core/Interpreter.h"
#include "core/InputBindings.h"
#include "ui/GraphUI.h"
#include "core/FileWatcher.h"

// oF lifecycle entry point. Delegates to Session; frame order:
// inputBindings → session.update → interpreter.update → checkLiveReload

class ofApp : public ofBaseApp{

 	public:
 		void setup();
 		void update();
 		void draw();
         
        // Set configuration from command-line arguments
        void setCommandLineConfig(const std::string& configPath,
                                  const std::string& scriptOverride,
                                  const std::string& windowTitle,
                                  const std::string& cwd);

 		void keyPressed(int key);
 		void mouseMoved(int x, int y);
 		void mousePressed(int x, int y, int button);
 		void mouseDragged(int x, int y, int button);
 		void mouseReleased(int x, int y, int button);
 		void mouseScrolled(int x, int y, float scrollX, float scrollY);
 		void windowResized(int w, int h);
 		void dragEvent(ofDragInfo dragInfo);
 		void exit();

    void checkLiveReload();

    Session session;
    Interpreter interpreter;
    FileWatcher fileWatcher;

    // UI state
    bool showGui = true;
    GraphUI graphUI;
    
private:
    // Command-line configuration
    std::string m_configPath = "config.json";
    std::string m_scriptOverride;
    std::string m_windowTitle;
    std::string m_cwd;
    
    // Active script path (for live-reload)
    std::string m_activeScriptPath;
};
