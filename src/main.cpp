#include "ofMain.h"
#include "ofApp.h"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>

namespace fs = std::filesystem;

static std::vector<pid_t> g_children;

static void forwardSignal(int sig) {
    for (auto pid : g_children) kill(pid, sig);
    _exit(128 + sig);
}

static std::string resolveExecutable(char* argv0) {
    char resolved[PATH_MAX];
    if (realpath(argv0, resolved)) return resolved;
    return argv0;
}

static fs::path resolveDataDir(const std::string& dir) {
    fs::path p(dir);
    if (p.is_absolute()) return p;
    fs::path candidate = fs::current_path() / "bin" / "data" / dir;
    if (fs::is_directory(candidate)) return candidate;
    candidate = fs::current_path() / dir;
    if (fs::is_directory(candidate)) return candidate;
    return p;
}

static int runAll(const std::string& dir, const std::string& configPath, char* argv0) {
    fs::path searchDir = resolveDataDir(dir);
    if (!fs::is_directory(searchDir)) {
        std::cerr << "Crumble: not a directory: " << searchDir << std::endl;
        return 1;
    }

    std::vector<fs::path> scripts;
    for (const auto& entry : fs::directory_iterator(searchDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            scripts.push_back(entry.path());
        }
    }
    if (scripts.empty()) {
        std::cerr << "Crumble: no .lua files in " << searchDir << std::endl;
        return 1;
    }
    std::sort(scripts.begin(), scripts.end());

    signal(SIGINT, forwardSignal);
    signal(SIGTERM, forwardSignal);

    std::string execPath = resolveExecutable(argv0);

    for (const auto& script : scripts) {
        std::string sArg = dir + script.filename().string();
        std::string tArg = script.stem().string();

        pid_t pid = fork();
        if (pid == 0) {
            char* args[] = {
                (char*)execPath.c_str(),
                (char*)"-s", (char*)sArg.c_str(),
                (char*)"-t", (char*)tArg.c_str(),
                (char*)"-c", (char*)configPath.c_str(),
                nullptr
            };
            execv(execPath.c_str(), args);
            perror("execv failed");
            _exit(1);
        } else if (pid > 0) {
            g_children.push_back(pid);
            std::cout << "Crumble: launched " << tArg << " (PID " << pid << ")" << std::endl;
        } else {
            perror("fork failed");
        }
    }

    int status;
    while (!g_children.empty()) {
        pid_t finished = wait(&status);
        auto it = std::find(g_children.begin(), g_children.end(), finished);
        if (it != g_children.end()) {
            std::cout << "Crumble: " << "process " << finished << " exited" << std::endl;
            g_children.erase(it);
        }
    }

    std::cout << "Crumble: all instances closed" << std::endl;
    return 0;
}

//========================================================================
// Command-line options:
//   -c, --config <path>   Config file path (default: config.json)
//   -s, --script <path>   Override entry script (default: from config)
//   -t, --title <name>    Window title
//   -a, --run-all <dir>   Launch one instance per .lua file in directory
//========================================================================
int main(int argc, char *argv[]){
    std::string configPath = "config.json";
    std::string scriptOverride;
    std::string windowTitle;
    std::string runAllDir;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            configPath = argv[++i];
        } else if ((arg == "-s" || arg == "--script") && i + 1 < argc) {
            scriptOverride = argv[++i];
        } else if ((arg == "-t" || arg == "--title") && i + 1 < argc) {
            windowTitle = argv[++i];
        } else if ((arg == "-a" || arg == "--run-all") && i + 1 < argc) {
            runAllDir = argv[++i];
        }
    }

    if (!runAllDir.empty()) {
        return runAll(runAllDir, configPath, argv[0]);
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