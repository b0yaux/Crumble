#pragma once
#include "ofMain.h"
#include "ofxLua.h"
#include "Session.h"

// Global Interpreter pointer for callback access
extern class Interpreter* g_interpreter;

/**
 * Interpreter executes the Lua live-coding DSL and maps it to C++ engine actions.
 * 
 * Design:
 * 1. Handle-based: Lua refers to nodes by their stable integer IDs (nodeId).
 * 2. Fire-and-Forget: Scripts execute once to build/update the graph, then exit.
 *    This avoids dangling pointers and GC issues with C++ objects in Lua.
 * 3. Reactive Binding: Maps the high-level Lua DSL to C++ Session/Node actions.
 * 4. Nested Execution: Supports recursive graph building via childGraph context.
 */
class Interpreter : public ofxLuaListener {
public:
    Interpreter();
    ~Interpreter();

    void setup(Session* session);
    
    // Add a directory to Lua's package.path for require() support
    void addScriptPath(const std::string& dir);
    
    // Execute a script to build or modify the current session
    bool runScript(const std::string& path);
    
    // Execute script in a nested graph context
    bool runScriptInGraph(const std::string& path, Graph* nestedGraph);
    
    // Execute a script in a nested graph context (static, callable from Session)
    static void executeInNestedGraph(const std::string& path, Graph* nestedGraph);
    
    static Session* s_currentSession;

    // Execute multiple scripts in order
    bool runScripts(const std::vector<std::string>& paths);
    
    // Call the global 'update(t)' function if it exists
    void update(const Transport& t);
    
    static void unref(int ref);
    
    // ofxLuaListener callbacks
    void errorReceived(std::string& msg) override;

private:
    Session* session = nullptr;
    ofxLua lua;

    // Internal binding helpers
    void bindSessionAPI();
    
    // Get the current graph (root or nested context)
    static Graph* getCurrentGraph();
    
    // Context stack for nested graph execution
    static thread_local std::vector<Graph*> s_graphStack;
    
    // RAII helper for graph context switching
    struct GraphContext {
        GraphContext(Graph* graph) { s_graphStack.push_back(graph); }
        ~GraphContext() { if (!s_graphStack.empty()) s_graphStack.pop_back(); }
    };
    
    // Lua-callable wrappers
    
    // Bridge functions (C-style for Lua)
    static int lua_addNode(lua_State* L);
    static int lua_removeNode(lua_State* L);
    // Find the lowest available input slot for a node's connections.
    // Stale connections (marked during hot-reload) are skipped so they can be
    // re-activated at their original slot instead of shifting to a new one.
    static int lua_nextInput(lua_State* L);
    static int lua_connect(lua_State* L);
    static int lua_getParam(lua_State* L);
    static int lua_setParam(lua_State* L);
    static int lua_clear(lua_State* L);
    static int lua_resolvePath(lua_State* L);
    static int lua_setAlias(lua_State* L);
    static int lua_setGenerator(lua_State* L);
    static int lua_setActive(lua_State* L);
    static int lua_getBank(lua_State* L);
    static int lua_setTempo(lua_State* L);
    static int lua_playhead(lua_State* L);
    static int lua_outlet(lua_State* L);
    static int lua_inlet(lua_State* L);
    static int lua_exposeParam(lua_State* L);

    static void restoreAutoNames(lua_State* L, int savedAutoNames, int savedAutoIndices, int savedNameCount);
};
