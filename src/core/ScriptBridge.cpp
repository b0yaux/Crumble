#include "ScriptBridge.h"

Session* ScriptBridge::s_currentSession = nullptr;

ScriptBridge::ScriptBridge() {
}

ScriptBridge::~ScriptBridge() {
    lua.removeListener(this);
}

void ScriptBridge::setup(Session* s) {
    session = s;
    lua.init();
    lua.addListener(this);
    
    // Add scripts directory to Lua's package.path for require()
    std::string scriptsPath = ofToDataPath("scripts", true);
    ofLogNotice("ScriptBridge") << "Scripts path: " << scriptsPath;
    std::string pathSetup = "package.path = '" + scriptsPath + "/?.lua;" + scriptsPath + "/?.lua;' .. package.path";
    ofLogNotice("ScriptBridge") << "Path setup: " << pathSetup;
    lua.doString(pathSetup);
    
    bindSessionAPI();
}

bool ScriptBridge::runScript(const std::string& path) {
    if (!session) return false;
    
    s_currentSession = session;
    
    ofFile script(path);
    if (!script.exists()) {
        ofLogError("ScriptBridge") << "Script not found: " << path;
        return false;
    }
    
    // Prepare for idempotent script execution
    session->beginScript();
    
    // Execute the script
    lua.doScript(path);
    
    // Remove nodes that weren't touched
    session->endScript();
    
    s_currentSession = nullptr;
    return true;
}

bool ScriptBridge::runScripts(const std::vector<std::string>& paths) {
    if (!session) return false;
    
    bool allSuccess = true;
    for (const auto& path : paths) {
        ofLogNotice("ScriptBridge") << "Loading script: " << path;
        if (!runScript(path)) {
            ofLogError("ScriptBridge") << "Failed to load script: " << path;
            allSuccess = false;
        }
    }
    return allSuccess;
}

void ScriptBridge::errorReceived(std::string& msg) {
    ofLogError("ScriptBridge") << "Lua Error: " << msg;
}

void ScriptBridge::bindSessionAPI() {
    lua_State* L = lua; // Uses the operator lua_State*() from ofxLua
    
    // Register global C functions
    lua_register(L, "_addNode", lua_addNode);
    lua_register(L, "_connect", lua_connect);
    lua_register(L, "_set", lua_setParam);
    lua_register(L, "_clear", lua_clear);
    lua_register(L, "_listDir", lua_listDirectory);
    lua_register(L, "_exists", lua_fileExists);
    
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

        _G.fileExists = _exists
        
        session.checkpoint = function() end

        session.setParam = function(s, a, b, c)
            local id, name, value = _getArgs(s, a, b, c)
            local targetId = type(id) == "table" and id.id or id
            _set(targetId, name, value)
        end

-- Directory Import Helper
        function importFolder(path, extension)
            if _exists(path) == false then
                return {}
            end

            local files = _listDir(path)
            local files = _listDir(path)
            local imported = {}
            
            -- Default supported extensions if none provided
            local filter = extension
            if not filter then
                filter = {".mov", ".hap", ".mp4", ".avi", ".wav", ".mp3", ".aif"}
            elseif type(filter) == "string" then
                filter = {filter}
            end

            for i, f in ipairs(files) do
                local ext = f:match("^.+(%..+)$")
                local match = false
                
                if ext then
                    for _, allowed in ipairs(filter) do
                        if ext:lower() == allowed:lower() then
                            match = true
                            break
                        end
                    end
                end

                if match then
                    local name = f:match("([^/]+)%..+$") or f
                    -- Determine node type based on extension (simple heuristic)
                    local nodeType = "VideoFileSource"
                    if ext:match("%.wav") or ext:match("%.mp3") or ext:match("%.aif") then
                        nodeType = "AudioFileSource"
                    end

                    local node = addNode(nodeType, name)
                    node.path = f
                    table.insert(imported, node)
                end
            end
            return imported
        end
    )";
    
    lua.doString(helper);
}

int ScriptBridge::lua_listDirectory(lua_State* L) {
    std::string path = luaL_checkstring(L, 1);
    ofDirectory dir(path);
    
    // No hardcoded allowExt() here. Return all files 
    // and let Lua handle the filtering logic.
    dir.listDir();
    
    lua_newtable(L);
    for(int i=0; i<(int)dir.size(); i++) {
        lua_pushinteger(L, i+1);
        lua_pushstring(L, dir.getPath(i).c_str());
        lua_settable(L, -3);
    }
    
    return 1;
}

int ScriptBridge::lua_fileExists(lua_State* L) {
    std::string path = luaL_checkstring(L, 1);
    ofFile file(path);
    lua_pushboolean(L, file.exists());
    return 1;
}

int ScriptBridge::lua_addNode(lua_State* L) {
    if (!s_currentSession) return 0;
    
    std::string type = luaL_checkstring(L, 1);
    std::string name = "";
    if (lua_gettop(L) >= 2) name = luaL_checkstring(L, 2);
    
    // Idempotent: if name provided and node with that name exists, return existing
    if (!name.empty()) {
        if (Node* existing = s_currentSession->findNodeByName(name)) {
            if (existing->type == type) {
                s_currentSession->touchNode(existing->nodeId);
                lua_pushinteger(L, existing->nodeId);
                return 1;
            }
        }
    }
    
    Node* node = s_currentSession->addNode(type, name);
    if (node) {
        s_currentSession->touchNode(node->nodeId);
        lua_pushinteger(L, node->nodeId);
        return 1;
    }
    return 0;
}

int ScriptBridge::lua_connect(lua_State* L) {
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

int ScriptBridge::lua_setParam(lua_State* L) {
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
            double val = lua_tonumber(L, 3);
            // Check if it's an int or float parameter
            if (p.type() == typeid(ofParameter<int>).name()) {
                p.cast<int>() = (int)val;
            } else if (p.type() == typeid(ofParameter<float>).name()) {
                p.cast<float>() = (float)val;
            } else if (p.type() == typeid(ofParameter<double>).name()) {
                p.cast<double>() = val;
            } else if (p.type() == typeid(ofParameter<bool>).name()) {
                p.cast<bool>() = (val > 0.5);
            }
        } else if (lua_isstring(L, 3)) {
            p.fromString(lua_tostring(L, 3));
        }
    }
    
    return 0;
}

int ScriptBridge::lua_clear(lua_State* L) {
    if (!s_currentSession) return 0;
    s_currentSession->clear();
    return 0;
}
