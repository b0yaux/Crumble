#pragma once
#include "ofMain.h"
#include "core/Session.h"
#include "core/Interpreter.h"
#include "ui/GraphUI.h"
#include "core/FileWatcher.h"

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
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
};
