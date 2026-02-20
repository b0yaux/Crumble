#include "ScriptEngine.h"

Session* ScriptEngine::s_currentSession = nullptr;

ScriptEngine::ScriptEngine() {
}

ScriptEngine::~ScriptEngine() {
    lua.removeListener(this);
}

void ScriptEngine::setup(Session* s) {
    session = s;
    lua.init();
    lua.addListener(this);
    
    bindSessionAPI();
}

bool ScriptEngine::runScript(const std::string& path) {
    if (!session) return false;
    
    s_currentSession = session;
    
    // Fire-and-forget: we clear and rebuild (or the script can choose to modify)
    // Most live-coding scripts will start with session.clear()
    
    ofFile script(path);
    if (!script.exists()) {
        ofLogError("ScriptEngine") << "Script not found: " << path;
        return false;
    }
    
    // Execute the script
    lua.doScript(path);
    
    s_currentSession = nullptr;
    return true;
}

void ScriptEngine::errorReceived(std::string& msg) {
    ofLogError("ScriptEngine") << "Lua Error: " << msg;
}

void ScriptEngine::bindSessionAPI() {
    lua_State* L = lua; // Uses the operator lua_State*() from ofxLua
    
    // Register global C functions
    lua_register(L, "_addNode", lua_addNode);
    lua_register(L, "_connect", lua_connect);
    lua_register(L, "_set", lua_setParam);
    lua_register(L, "_clear", lua_clear);
    
    // Create the 'session' table and 'Node' metatable in Lua
    std::string helper = R"(
        session = {}
        
        local NodeMeta = {}
        
        -- Support for node.parameter = value
        function NodeMeta:__newindex(key, value)
            if key == "id" or key == "type" or key == "name" then
                rawset(self, key, value)
            else
                _set(self.id, key, value)
            end
        end

        -- Support for value = node.parameter
        NodeMeta.__index = function(t, k)
            if k == "set" then 
                return function(self, name, val) _set(self.id, name, val); return self end
            elseif k == "connect" then
                return function(self, toNode, fromOut, toIn)
                    local toId = type(toNode) == "table" and toNode.id or toNode
                    _connect(self.id, toId, fromOut or 0, toIn or 0)
                    return self
                end
            end
            return rawget(NodeMeta, k)
        end

        -- Internal helper
        local function _getArgs(s, a, b)
            if s == session then return a, b end
            return s, a
        end

        -- Global shortcuts and session methods
        local function addNodeInternal(t, n)
            if type(t) ~= "string" then
                error("addNode: first argument must be a string (type), got " .. type(t), 2)
            end
            local id = _addNode(t, n or "")
            if id then
                local nodeObj = { id = id, type = t, name = n }
                setmetatable(nodeObj, NodeMeta)
                return nodeObj
            end
        end
        
        _G.addNode = addNodeInternal
        
        session.addNode = function(s, t, n)
            if s == session then return addNodeInternal(t, n) end
            return addNodeInternal(s, t)
        end

        local function connectInternal(f, t, fo, ti)
            local fid = type(f) == "table" and f.id or f
            local tid = type(t) == "table" and t.id or t
            _connect(fid, tid, fo or 0, ti or 0)
        end

        _G.connect = connectInternal
        
        session.connect = function(s, f, t, fo, ti)
            if s == session then return connectInternal(f, t, fo, ti) end
            return connectInternal(s, f, t, fo)
        end

        _G.clear = _clear
        session.clear = function(s)
            _clear()
        end

        session.checkpoint = function() end

        session.setParam = function(s, a, b, c)
            local id, name, value = _getArgs(s, a, b, c)
            local targetId = type(id) == "table" and id.id or id
            _set(targetId, name, value)
        end
    )";
    
    lua.doString(helper);
}

int ScriptEngine::lua_addNode(lua_State* L) {
    if (!s_currentSession) return 0;
    
    std::string type = luaL_checkstring(L, 1);
    std::string name = "";
    if (lua_gettop(L) >= 2) name = luaL_checkstring(L, 2);
    
    Node* node = s_currentSession->addNode(type, name);
    if (node) {
        lua_pushinteger(L, node->nodeIndex);
        return 1;
    }
    return 0;
}

int ScriptEngine::lua_connect(lua_State* L) {
    if (!s_currentSession) return 0;
    
    int fromNode = luaL_checkinteger(L, 1);
    int toNode = luaL_checkinteger(L, 2);
    int fromOut = 0;
    int toIn = 0;
    
    if (lua_gettop(L) >= 3) fromOut = luaL_checkinteger(L, 3);
    if (lua_gettop(L) >= 4) toIn = luaL_checkinteger(L, 4);
    
    s_currentSession->connect(fromNode, toNode, fromOut, toIn);
    return 0;
}

int ScriptEngine::lua_setParam(lua_State* L) {
    if (!s_currentSession) return 0;
    
    int nodeIdx = luaL_checkinteger(L, 1);
    std::string paramName = luaL_checkstring(L, 2);
    
    Node* node = s_currentSession->getNode(nodeIdx);
    if (!node) return 0;
    
    // Try to find the parameter
    if (node->parameters.contains(paramName)) {
        auto& p = node->parameters.get(paramName);
        
        if (lua_isboolean(L, 3)) {
            p.cast<bool>() = lua_toboolean(L, 3);
        } else if (lua_isnumber(L, 3)) {
            // Check if it's an int or float parameter
            if (p.type() == typeid(ofParameter<int>).name()) {
                p.cast<int>() = (int)lua_tonumber(L, 3);
            } else if (p.type() == typeid(ofParameter<float>).name()) {
                p.cast<float>() = (float)lua_tonumber(L, 3);
            } else if (p.type() == typeid(ofParameter<double>).name()) {
                p.cast<double>() = (double)lua_tonumber(L, 3);
            }
        } else if (lua_isstring(L, 3)) {
            p.fromString(lua_tostring(L, 3));
        }
    }
    
    return 0;
}

int ScriptEngine::lua_clear(lua_State* L) {
    if (!s_currentSession) return 0;
    s_currentSession->clear();
    return 0;
}
