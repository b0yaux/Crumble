#include "ofMain.h"
#include "ofApp.h"

//========================================================================
// Command-line options:
//   -c, --config <path>   Config file path (default: config.json)
//   -s, --script <path>   Override entry script (default: from config)
//   -t, --title <name>    Window title
//========================================================================
int main(int argc, char *argv[]){
    std::string configPath = "config.json";
    std::string scriptOverride;
    std::string windowTitle;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            configPath = argv[++i];
        } else if ((arg == "-s" || arg == "--script") && i + 1 < argc) {
            scriptOverride = argv[++i];
        } else if ((arg == "-t" || arg == "--title") && i + 1 < argc) {
            windowTitle = argv[++i];
        }
    }
    
	ofGLFWWindowSettings settings;
	settings.setSize(1024, 768);
	settings.windowMode = OF_WINDOW;
    settings.setGLVersion(4, 1);
	auto mainWindow = ofCreateWindow(settings);

    auto app = std::make_shared<ofApp>();
    app->setCommandLineConfig(configPath, scriptOverride, windowTitle);
	ofRunApp(mainWindow, app);
	ofRunMainLoop();
}