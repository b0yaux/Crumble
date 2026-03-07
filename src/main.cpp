#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main( ){
	ofGLFWWindowSettings settings;
	settings.setSize(1024, 768);
	settings.windowMode = OF_WINDOW;
    settings.setGLVersion(4, 1);
	auto mainWindow = ofCreateWindow(settings);

    auto app = std::make_shared<ofApp>();
	ofRunApp(mainWindow, app);
	ofRunMainLoop();
}