#pragma once
#include "ofMain.h"
#include "ofxLua.h"
#include "Session.h"

/**
 * ScriptBridge facilitates communication between the Lua scripting environment 
 * and the Crumble C++ engine.
 * 
 * Design:
 * 1. Handle-based: Lua refers to nodes by their stable integer IDs (nodeId).
 * 2. Fire-and-Forget: Scripts execute once to build/update the graph, then exit.
 *    This avoids dangling pointers and GC issues with C++ objects in Lua.
 * 3. Reactive Binding: Maps the high-level Lua DSL to C++ Session/Node actions.
 */
class ScriptBridge : public ofxLuaListener {
public:
    ScriptBridge();
    ~ScriptBridge();

    void setup(Session* session);
    
    // Execute a script to build or modify the current session
    bool runScript(const std::string& path);
    
    // Execute multiple scripts in order
    bool runScripts(const std::vector<std::string>& paths);
    
    // ofxLuaListener callbacks
    void errorReceived(std::string& msg) override;

private:
    Session* session = nullptr;
    ofxLua lua;

    // Internal binding helpers
    void bindSessionAPI();
    
    // Lua-callable wrappers
    // We use a static pointer to the current session for the Lua C-functions
    static Session* s_currentSession;
    
    // Bridge functions (C-style for Lua)
    static int lua_addNode(lua_State* L);
    static int lua_connect(lua_State* L);
    static int lua_setParam(lua_State* L);
    static int lua_clear(lua_State* L);
    static int lua_listDirectory(lua_State* L);
    static int lua_fileExists(lua_State* L);
};
