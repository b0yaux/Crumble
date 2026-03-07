#pragma once
#include "ofMain.h"
#include "ofxLua.h"
#include "Session.h"
#include "Graph.h"

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
    
    // Execute a script to build or modify the current session
    bool runScript(const std::string& path);
    
    // Execute script in a nested graph context
    bool runScriptInGraph(const std::string& path, Graph* nestedGraph);
    
    // Execute a script in a nested graph context (static, callable from Session)
    static void executeInNestedGraph(const std::string& path, Graph* nestedGraph);
    
    // Execute multiple scripts in order
    bool runScripts(const std::vector<std::string>& paths);
    
    // Call the global 'update(t)' function if it exists
    void update(const Transport& t);
    
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
    static Session* s_currentSession;
    
    // RAII helper for graph context switching
    struct GraphContext {
        GraphContext(Graph* graph) { s_graphStack.push_back(graph); }
        ~GraphContext() { if (!s_graphStack.empty()) s_graphStack.pop_back(); }
    };
    
    // Lua-callable wrappers
    
    // Bridge functions (C-style for Lua)
    static int lua_addNode(lua_State* L);
    static int lua_connect(lua_State* L);
    static int lua_getParam(lua_State* L);
    static int lua_setParam(lua_State* L);
    static int lua_clear(lua_State* L);
    static int lua_listDirectory(lua_State* L);
    static int lua_fileExists(lua_State* L);
    static int lua_resolvePath(lua_State* L);
    static int lua_setGenerator(lua_State* L);
    static int lua_getBank(lua_State* L);
};
