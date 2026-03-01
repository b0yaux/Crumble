#include "ScriptBridge.h"
#include "Session.h"

ScriptBridge* g_scriptBridge = nullptr;

Session* ScriptBridge::s_currentSession = nullptr;
thread_local std::vector<Graph*> ScriptBridge::s_graphStack;

ScriptBridge::ScriptBridge() {
    g_scriptBridge = this;
}

ScriptBridge::~ScriptBridge() {
    lua.removeListener(this);
}

void ScriptBridge::setup(Session* s) {
    session = s;
    
    // Register the nested script executor in Graph
    Graph::setScriptExecutor([](const std::string& path, Graph* nestedGraph) {
        executeInNestedGraph(path, nestedGraph);
    });
    
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

Graph* ScriptBridge::getCurrentGraph() {
    if (!s_graphStack.empty()) {
        return s_graphStack.back();
    }
    return &s_currentSession->getGraph();
}

bool ScriptBridge::runScript(const std::string& path) {
    if (!session) return false;
    
    s_currentSession = session;
    
    ofFile script(path);
    if (!script.exists()) {
        ofLogError("ScriptBridge") << "Script not found: " << path;
        return false;
    }
    
    // Root graph context
    GraphContext ctx(&session->getGraph());
    
    // Prepare for idempotent script execution
    session->beginScript();
    
    // Execute the script
    lua.doScript(path);
    
    // Remove nodes that weren't touched
    session->endScript();
    
    s_currentSession = nullptr;
    return true;
}

bool ScriptBridge::runScriptInGraph(const std::string& path, Graph* nestedGraph) {
    if (!session || !nestedGraph) return false;
    
    s_currentSession = session;
    
    ofFile script(path);
    if (!script.exists()) {
        ofLogError("ScriptBridge") << "Script not found: " << path;
        return false;
    }
    
    GraphContext ctx(nestedGraph);
    lua.doScript(path);
    
    return true;
}

void ScriptBridge::executeInNestedGraph(const std::string& path, Graph* nestedGraph) {
    if (!nestedGraph || !g_scriptBridge) return;
    
    ofFile script(path);
    if (!script.exists()) {
        ofLogError("ScriptBridge") << "Nested script not found: " << path;
        return;
    }
    
    g_scriptBridge->runScriptInGraph(path, nestedGraph);
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

void ScriptBridge::update(const Transport& t) {
    if (!session) return;
    
    // Set static session pointer so _set and _get bindings work during update()
    s_currentSession = session;
    
    lua_State* L = lua; // Uses ofxLua conversion operator
    
    // 1. Update global 'Time' table (create if not exists)
    lua_getglobal(L, "Time");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "Time");
    }
    
    lua_pushnumber(L, t.bpm);
    lua_setfield(L, -2, "bpm");
    
    lua_pushnumber(L, t.absoluteTime);
    lua_setfield(L, -2, "absoluteTime");
    
    lua_pushnumber(L, t.cycle);
    lua_setfield(L, -2, "cycle");
    
    lua_pushboolean(L, t.isPlaying);
    lua_setfield(L, -2, "isPlaying");
    
    // 2. Check if 'update' function exists in global scope
    lua_getglobal(L, "update");
    if (lua_isfunction(L, -1)) {
        // Push the 'Time' table again as an argument
        lua_pushvalue(L, -2);
        
        // Call update(Time)
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            std::string err = lua_tostring(L, -1);
            ofLogError("ScriptBridge") << "Error in update loop: " << err;
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1); // Pop if not a function
    }
    
    lua_pop(L, 1); // Pop 'Time' table
    
    // Reset it
    s_currentSession = nullptr;
}

void ScriptBridge::errorReceived(std::string& msg) {
    ofLogError("ScriptBridge") << "Lua Error: " << msg;
}

void ScriptBridge::bindSessionAPI() {
    lua_State* L = lua; // Uses the operator lua_State*() from ofxLua
    
    // Register global C functions
    lua_register(L, "_addNode", lua_addNode);
    lua_register(L, "_connect", lua_connect);
    lua_register(L, "_get", lua_getParam);
    lua_register(L, "_set", lua_setParam);
    lua_register(L, "_setGen", lua_setGenerator);
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
            elseif type(value) == "table" and value._isGen then
                _setGen(self.id, key, value)
            else
                _set(self.id, key, value)
            end
        end

        -- Support for value = node.parameter
        NodeMeta.__index = function(t, k)
            if k == "set" then 
                return function(self, name, val) 
                    if type(val) == "table" and val._isGen then
                        _setGen(self.id, name, val)
                    else
                        _set(self.id, name, val)
                    end
                    return self 
                end
            elseif k == "connect" then
                return function(self, toNode, fromOut, toIn)
                    local toId = type(toNode) == "table" and toNode.id or toNode
                    _connect(self.id, toId, fromOut or 0, toIn or 0)
                    return self
                end
            else
                -- Try to get parameter value from node
                return _get(t.id, k)
            end
        end

        -- Internal helper
        local function _getArgs(s, a, b)
            if s == session then return a, b end
            return s, a
        end

        -- Global shortcuts and session methods
        -- Track all nodes for glob pattern matching
        _G._allNodes = {}
        
        local function addNodeInternal(t, n)
            if type(t) ~= "string" then
                error("addNode: first argument must be a string (type), got " .. type(t), 2)
            end
            local id = _addNode(t, n or "")
            if id then
                local nodeObj = { id = id, type = t, name = n or t .. "_" .. id }
                table.insert(_G._allNodes, nodeObj)
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
            local tid = type(t) == "table" and t.id or t
            local toIdx = ti or 0
            local fromIdx = fo or 0
            
            -- Handle array of sources: connect({smp1, smp2}, mixer)
            -- Check it's a proper array (has numeric keys starting at 1 with table values)
            if type(f) == "table" and f[1] and type(f[1]) == "table" and f[1].id then
                for _, src in ipairs(f) do
                    if src and src.id then  -- Skip nil entries
                        _connect(src.id, tid, fromIdx, toIdx)
                        toIdx = toIdx + 1
                    end
                end
                return
            end
            
            -- Handle glob pattern: connect("smp*", mixer)
            if type(f) == "string" and f:match("%*") then
                local pattern = f:gsub("%*", ".*")
                local matched = false
                for _, node in ipairs(_G._allNodes or {}) do
                    if node.name:match(pattern) or node.type:match(pattern) then
                        _connect(node.id, tid, fromIdx, toIdx)
                        toIdx = toIdx + 1
                        matched = true
                    end
                end
                if matched then return end
            end
            
            -- Single node connection
            local fid = type(f) == "table" and f.id or f
            _connect(fid, tid, fromIdx, toIdx)
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
            if type(value) == "table" and value._isGen then
                _setGen(targetId, name, value)
            else
                _set(targetId, name, value)
            end
        end

        -- Musical Pattern DSL
        GeneratorMeta = {}
        GeneratorMeta.__index = GeneratorMeta

        function GeneratorMeta.__mul(a, b)
            local g = { _isGen = true, type = "mul", a = a, b = b }
            setmetatable(g, GeneratorMeta)
            return g
        end

        function GeneratorMeta.__add(a, b)
            local g = { _isGen = true, type = "add", a = a, b = b }
            setmetatable(g, GeneratorMeta)
            return g
        end

        _G.seq = function(p)
            local g = { _isGen = true, type = "seq", val = p }
            setmetatable(g, GeneratorMeta)
            return g
        end
        
        _G.osc = function(f)
            local g = { _isGen = true, type = "osc", val = f }
            setmetatable(g, GeneratorMeta)
            return g
        end
        
        _G.ramp = function(f)
            local g = { _isGen = true, type = "ramp", val = f }
            setmetatable(g, GeneratorMeta)
            return g
        end

-- Directory Import Helper
        function importFolder(path, extension)
            if _exists(path) == false then
                return {}
            end

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
    
    bool success = lua.doString(helper);
    if (!success) {
        ofLogError("ScriptBridge") << "Failed to evaluate Lua helper DSL!";
    }
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
    
    Graph* graph = getCurrentGraph();
    
    // Idempotent: if name provided and node with that name exists, return existing
    if (!name.empty()) {
        for (const auto& pair : graph->getNodes()) {
            Node* existing = pair.second.get();
            if (existing && existing->name == name && existing->type == type) {
                existing->touched = true;
                lua_pushinteger(L, existing->nodeId);
                return 1;
            }
        }
    }
    
    Node* node = graph->createNode(type, name);
    if (node) {
        node->touched = true;
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
    
    Graph* graph = getCurrentGraph();
    graph->connect(fromNode, toNode, fromOut, toIn);
    return 0;
}

int ScriptBridge::lua_setParam(lua_State* L) {
    if (!s_currentSession) return 0;
    
    int nodeIdx = luaL_checkinteger(L, 1);
    std::string paramName = luaL_checkstring(L, 2);
    
    Graph* graph = getCurrentGraph();
    Node* node = graph->getNode(nodeIdx);
    if (!node) return 0;
    
    // Clear any existing modulator so static value takes precedence!
    node->clearModulator(paramName);
    
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
        
        // Notify the node that a parameter changed (enables reactive behavior)
        node->onParameterChanged(paramName);
    }
    
    return 0;
}

// Internal helper to recursively build C++ Generators from Lua tables
std::shared_ptr<Generator<float>> parseGenerator(lua_State* L, int index) {
    if (index < 0) index = lua_gettop(L) + index + 1;
    
    int type = lua_type(L, index);

    if (type == LUA_TNUMBER) {
        float val = (float)lua_tonumber(L, index);
        return std::make_shared<ConstGenerator>(val);
    }
    
    if (type == LUA_TTABLE) {
        lua_getfield(L, index, "type");
        const char* typeStr = lua_tostring(L, -1);
        std::string genType = typeStr ? typeStr : "";
        lua_pop(L, 1);
        
        if (genType == "seq") {
            lua_getfield(L, index, "val");
            const char* patStr = lua_tostring(L, -1);
            std::string pattern = patStr ? patStr : "";
            lua_pop(L, 1);
            return std::make_shared<SeqGenerator>(pattern);
        } else if (genType == "osc") {
            lua_getfield(L, index, "val");
            float freq = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
            return std::make_shared<OscGenerator>(freq);
        } else if (genType == "ramp") {
            lua_getfield(L, index, "val");
            float freq = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
            return std::make_shared<RampGenerator>(freq);
        } else if (genType == "mul") {
            lua_getfield(L, index, "a");
            auto genA = parseGenerator(L, lua_gettop(L));
            lua_pop(L, 1);
            lua_getfield(L, index, "b");
            auto genB = parseGenerator(L, lua_gettop(L));
            lua_pop(L, 1);
            if (genA && genB) return std::make_shared<MulGenerator>(genA, genB);
        } else if (genType == "add") {
            lua_getfield(L, index, "a");
            auto genA = parseGenerator(L, lua_gettop(L));
            lua_pop(L, 1);
            lua_getfield(L, index, "b");
            auto genB = parseGenerator(L, lua_gettop(L));
            lua_pop(L, 1);
            if (genA && genB) return std::make_shared<AddGenerator>(genA, genB);
        }
    }
    
    return nullptr;
}

int ScriptBridge::lua_setGenerator(lua_State* L) {
    if (!s_currentSession) return 0;
    
    int nodeIdx = (int)luaL_checkinteger(L, 1);
    std::string paramName = luaL_checkstring(L, 2);
    
    Graph* graph = getCurrentGraph();
    Node* node = graph->getNode(nodeIdx);
    if (!node) return 0;

    auto gen = parseGenerator(L, 3);
    if (gen) {
        node->setModulator(paramName, gen);
    }

    node->onParameterChanged(paramName);
    return 0;
}

int ScriptBridge::lua_getParam(lua_State* L) {
    if (!s_currentSession) return 0;
    
    int nodeIdx = luaL_checkinteger(L, 1);
    std::string paramName = luaL_checkstring(L, 2);
    
    Graph* graph = getCurrentGraph();
    Node* node = graph->getNode(nodeIdx);
    if (!node) return 0;
    
    // Try to find the parameter
    if (node->parameters.contains(paramName)) {
        auto& p = node->parameters.get(paramName);
        
        // Push the parameter value to Lua stack
        if (p.type() == typeid(ofParameter<bool>).name()) {
            lua_pushboolean(L, p.cast<bool>());
        } else if (p.type() == typeid(ofParameter<int>).name()) {
            lua_pushinteger(L, p.cast<int>());
        } else if (p.type() == typeid(ofParameter<float>).name()) {
            lua_pushnumber(L, p.cast<float>());
        } else if (p.type() == typeid(ofParameter<double>).name()) {
            lua_pushnumber(L, p.cast<double>());
        } else if (p.type() == typeid(ofParameter<std::string>).name()) {
            std::string val = p.cast<std::string>();
            lua_pushstring(L, val.c_str());
        } else {
            // Fallback: try to convert to string
            lua_pushstring(L, p.toString().c_str());
        }
        return 1;
    }
    
    // Parameter not found, return nil
    return 0;
}

int ScriptBridge::lua_clear(lua_State* L) {
    if (!s_currentSession) return 0;
    Graph* graph = getCurrentGraph();
    graph->clear();
    return 0;
}
