#pragma once
#include "ofMain.h"
#include "ofxLua.h"
#include "Session.h"

/**
 * ScriptEngine provides a Lua live-scripting bridge for Crumble.
 * 
 * Design:
 * 1. Handle-based: Lua refers to nodes by their integer indices (nodeIndex).
 * 2. Fire-and-Forget: Scripts execute once to build/update the graph, then exit.
 *    This avoids dangling pointers and GC issues with C++ objects in Lua.
 * 3. Two-way: Can reconstruct a graph from Lua, or export the current graph to a Lua script.
 */
class ScriptEngine : public ofxLuaListener {
public:
    ScriptEngine();
    ~ScriptEngine();

    void setup(Session* session);
    
    // Execute a script to build or modify the current session
    bool runScript(const std::string& path);
    
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
};
