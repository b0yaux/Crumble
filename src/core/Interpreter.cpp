#include "Interpreter.h"
#include "Config.h"
#include "AssetRegistry.h"
#include "PatternMath.h"
#include "../nodes/AudioSource.h"
#include "../nodes/VideoSource.h"
#include <unordered_set>
#include <sstream>

Session* Interpreter::s_currentSession = nullptr;
thread_local std::vector<Graph*> Interpreter::s_graphStack;
Interpreter* g_interpreter = nullptr;

Interpreter::Interpreter() {
    g_interpreter = this;
    lua.addListener(this);
}

Interpreter::~Interpreter() {
    lua.removeListener(this);
    if (g_interpreter == this) g_interpreter = nullptr;
}

void Interpreter::setup(Session* s) {
    this->session = s;
    lua.init();
    
    // Add scripts folder to package.path for 'require' support
    std::string scriptsPath = ofToDataPath("scripts", true);
    ofLogNotice("Interpreter") << "Scripts path: " << scriptsPath;
    
    std::string pathSetup = "package.path = '" + scriptsPath + "/?.lua;" + scriptsPath + "/?.lua;' .. package.path";
    lua.doString(pathSetup);
    ofLogNotice("Interpreter") << "Path setup: " << pathSetup;
    
    // Connect Graph's static script executor to the Interpreter's recursive runner
    Graph::setScriptExecutor([](const std::string& path, Graph* g) {
        if (g_interpreter) g_interpreter->runScriptInGraph(path, g);
    });
    
    bindSessionAPI();
    
    std::string preludePath = ofToDataPath("system/prelude.lua", true);
    if (ofFile::doesFileExist(preludePath)) {
        lua.doScript(preludePath);
        ofLogNotice("Interpreter") << "Loaded standard library: prelude.lua";
    } else {
        ofLogWarning("Interpreter") << "System prelude not found at " << preludePath;
    }
}

void Interpreter::addScriptPath(const std::string& dir) {
    std::string normalizedDir = dir;
    if (!normalizedDir.empty() && normalizedDir.back() != '/') {
        normalizedDir += '/';
    }
    
    std::string pathSetup = "package.path = '" + normalizedDir + "?.lua;' .. package.path";
    lua.doString(pathSetup);
    ofLogNotice("Interpreter") << "Added to package.path: " << normalizedDir;
}

bool Interpreter::runScript(const std::string& path) {
    if (!session) return false;
    
    s_currentSession = session;
    GraphContext rootContext(&session->getGraph());
    
    // Mark all existing nodes as untouched for cleanup tracking
    session->beginScript();
    
    // Reset auto-indexing and connections for idempotency
    lua.doString("_autoNames = {}; _nameCount = {}");
    session->getGraph().markConnectionsStale();
    
    bool success = lua.doScript(path, true);
    
    // Clean up nodes that weren't touched (not re-created in script)
    session->endScript();
    // Clear modulators that survived reload but weren't re-applied by the new script
    session->getGraph().clearUntouchedModulators();
    session->getGraph().pruneStaleConnections();
    
    s_currentSession = nullptr;
    return success;
}

void Interpreter::unref(int ref) {
    if (g_interpreter && ref != LUA_NOREF) {
        luaL_unref(g_interpreter->lua, LUA_REGISTRYINDEX, ref);
    }
}

bool Interpreter::runScriptInGraph(const std::string& path, Graph* nestedGraph) {
    if (!session || !nestedGraph) return false;
    
    Session* saved = s_currentSession;
    s_currentSession = session;
    GraphContext ctx(nestedGraph);

    lua_State* L = lua;

    // 0. Clear ephemeral script state (outlets, proxyParams, Lua refs).
    //    Children are NOT destroyed — they survive for idempotent reuse.
    nestedGraph->clearEphemeral();

    // Save auto-name counters (shared global) so sub-graph reload
    // doesn't clobber the parent's or sibling sub-graphs' counters.
    lua_getglobal(L, "_autoNames");
    int savedAutoNames = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getglobal(L, "_nameCount");
    int savedNameCount = luaL_ref(L, LUA_REGISTRYINDEX);

    // Reset auto-name counters for stable name generation within this sub-graph
    lua.doString("_autoNames = {}; _nameCount = {}");

    // Idempotent reload: mark children, re-execute, GC untouched
    nestedGraph->beginScript();
    nestedGraph->markConnectionsStale();
    
    // 1. Create sub-graph environment table
    lua_newtable(L); 
    int envIdx = lua_gettop(L);
    
    // 2. env.__index = _G
    lua_newtable(L);
    lua_getglobal(L, "_G");
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, envIdx);
    
    // 3. Load script chunk
    if (luaL_loadfile(L, ofToDataPath(path).c_str()) != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        ofLogError("Lua") << "Error loading subgraph script: " << err;
        lua_pop(L, 2);
        restoreAutoNames(L, savedAutoNames, savedNameCount);
        s_currentSession = saved;
        return false;
    }
    
    // 4. Set chunk's _ENV = env
    lua_pushvalue(L, envIdx);
    lua_setupvalue(L, -2, 1);
    
    // 5. Run the script
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        ofLogError("Lua") << "Error running subgraph script: " << err;
        lua_pop(L, 2);
        nestedGraph->endScript();
        nestedGraph->clearUntouchedModulators();
        nestedGraph->pruneStaleConnections();
        restoreAutoNames(L, savedAutoNames, savedNameCount);
        s_currentSession = saved;
        return false;
    }

    // GC children that weren't re-declared by the script
    nestedGraph->endScript();
    nestedGraph->clearUntouchedModulators();
    nestedGraph->pruneStaleConnections();

    // 6. Check for 'update' inside the environment
    lua_getfield(L, envIdx, "update");
    int updateRef = LUA_NOREF;
    if (lua_isfunction(L, -1)) {
        updateRef = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_pop(L, 1);
    }
    
    // 7. Store the environment table itself as a reference
    int envRef = luaL_ref(L, LUA_REGISTRYINDEX);

    // 8. Bind lifecycle hooks
    nestedGraph->onUpdate = [this, updateRef, nestedGraph]() {
        if (updateRef == LUA_NOREF) return;
        Session* prevSession = s_currentSession;
        s_currentSession = session;
        GraphContext innerCtx(nestedGraph);
        lua_State* L = lua;
        lua_rawgeti(L, LUA_REGISTRYINDEX, updateRef);
        if (lua_isfunction(L, -1)) {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                std::string err = lua_tostring(L, -1);
                ofLogError("Lua") << "Error in subgraph update: " << err;
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }
        s_currentSession = prevSession;
    };

    nestedGraph->onClear = [updateRef, envRef]() {
        Interpreter::unref(updateRef);
        Interpreter::unref(envRef);
    };
    
    restoreAutoNames(L, savedAutoNames, savedNameCount);
    s_currentSession = saved;
    return true;
}

void Interpreter::restoreAutoNames(lua_State* L, int savedAutoNames, int savedNameCount) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, savedAutoNames);
    lua_setglobal(L, "_autoNames");
    luaL_unref(L, LUA_REGISTRYINDEX, savedAutoNames);

    lua_rawgeti(L, LUA_REGISTRYINDEX, savedNameCount);
    lua_setglobal(L, "_nameCount");
    luaL_unref(L, LUA_REGISTRYINDEX, savedNameCount);
}

void Interpreter::executeInNestedGraph(const std::string& path, Graph* nestedGraph) {
    if (g_interpreter) {
        g_interpreter->runScriptInGraph(path, nestedGraph);
    }
}

bool Interpreter::runScripts(const std::vector<std::string>& paths) {
    bool allSuccess = true;
    for (const auto& p : paths) {
        if (!runScript(p)) allSuccess = false;
    }
    return allSuccess;
}

void Interpreter::update(const Transport& t) {
    if (!session) return;
    
    s_currentSession = session;
    GraphContext rootContext(&session->getGraph());

    lua_State* L = lua;

    lua.doString("_nameCount = {}");

    // Create 'Time' table
    lua_newtable(L);
    
    lua_pushstring(L, "absoluteTime");
    lua_pushnumber(L, t.absoluteTime);
    lua_settable(L, -3);
    
    lua_pushstring(L, "abs");
    lua_pushnumber(L, t.absoluteTime);
    lua_settable(L, -3);
    
    lua_pushstring(L, "cycle");
    lua_pushnumber(L, t.cycle);
    lua_settable(L, -3);
    
    lua_pushstring(L, "bars");
    lua_pushnumber(L, t.bars);
    lua_settable(L, -3);
    
    lua_pushstring(L, "tempo");
    lua_pushnumber(L, t.bpm);
    lua_settable(L, -3);
    
    lua_setglobal(L, "Time");

    // Create 'Gamepad' table — snapshot of current gamepad state for Lua update()
    {
        lua_settop(L, 0); // Clear stack — guard against leaks from prior operations
        auto& ib = session->getInputBindings();
        lua_newtable(L);

        static const char* axisNames[] = {"lx", "ly", "rx", "ry", "lt", "rt"};
        for (int i = 0; i < 6; i++) {
            auto* b = ib.getBinding("gamepad:axis:" + std::to_string(i));
            float v = b ? b->load(std::memory_order_acquire) : 0.0f;
            lua_pushstring(L, axisNames[i]);
            lua_pushnumber(L, v);
            lua_settable(L, -3);
        }

        static const char* btnNames[] = {
            "cross", "circle", "square", "triangle",
            "select", "ps", "start", "l3", "r3",
            "l1", "r1", "up", "down", "left", "right"
        };
        for (int i = 0; i < 15; i++) {
            auto* b = ib.getBinding("gamepad:button:" + std::to_string(i));
            float v = b ? b->load(std::memory_order_acquire) : 0.0f;
            lua_pushstring(L, btnNames[i]);
            lua_pushnumber(L, v);
            lua_settable(L, -3);
        }

        lua_setglobal(L, "Gamepad");
    }

    // Call global update function if it exists
    lua_getglobal(L, "update");
    if (lua_isfunction(L, -1)) {
        // Push the Time table as argument
        lua_getglobal(L, "Time");
        
        // Call update(Time)
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            std::string err = lua_tostring(L, -1);
            ofLogError("Interpreter") << "Error in update loop: " << err;
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
    
    s_currentSession = nullptr;
}

void Interpreter::errorReceived(std::string& msg) {
    ofLogError("Interpreter") << "Lua Error: " << msg;
}

void Interpreter::bindSessionAPI() {
    lua_State* L = lua;
    
    lua_register(L, "_addNode", lua_addNode);
    lua_register(L, "_removeNode", lua_removeNode);
    lua_register(L, "_nextInput", lua_nextInput);
    lua_register(L, "_connect", lua_connect);
    lua_register(L, "_get", lua_getParam);
    lua_register(L, "_set", lua_setParam);
    lua_register(L, "_setGen", lua_setGenerator);
    lua_register(L, "_setActive", lua_setActive);
    lua_register(L, "_clear", lua_clear);
    lua_register(L, "_resolve", lua_resolvePath);
    lua_register(L, "_setAlias", lua_setAlias);
    lua_register(L, "_getBank", lua_getBank);
    lua_register(L, "_setTempo", lua_setTempo);
    lua_register(L, "_playhead", lua_playhead);
    lua_register(L, "_outlet", lua_outlet);
    lua_register(L, "_inlet", lua_inlet);
    lua_register(L, "_exposeParam", lua_exposeParam);
    lua_register(L, "_readBinding", lua_readBinding);
    
    std::string helper = R"(
        session = {}
        local NodeMeta = {}
        
        function bpm(v) _setTempo(v) end
        function cpm(v) _setTempo(v * 4) end
        function cps(v) _setTempo(v * 240) end

        
function NodeMeta:__newindex(key, value)
            if key == "id" or key == "type" or key == "name" then
                rawset(self, key, value)
            elseif type(value) == "string" then
                _set(self.id, key, value)
            elseif type(value) == "table" and value._isGen then
                _setGen(self.id, key, value)
            else
                _set(self.id, key, value)
            end
        end

        NodeMeta.__index = function(t, k)
            if k == "set" then 
                return function(self, name, val) 
                    if type(val) == "table" and val._isGen then _setGen(self.id, name, val)
                    else _set(self.id, name, val) end
                    return self 
                end
            elseif k == "connect" then
                return function(self, toNode, paramsOrOut, toIn)
                    connect(self, toNode, paramsOrOut, toIn)
                    return self
                end
            elseif k == "off" then return function(self) _setActive(self.id, false) return self end
            elseif k == "on" then return function(self) _setActive(self.id, true) return self end
            elseif k == "destroy" then return function(self) _removeNode(self.id) _G._allNodes[self.id] = nil end
            elseif k == "mute" then return function(self) _setActive(self.id, false) return self end
            elseif k == "unmute" then return function(self) _setActive(self.id, true) return self end
            elseif k == "outlet" then return function(self, idx) _outlet(self.id, idx or 0) return self end
            elseif k == "inlet" then return function(self, idx) _inlet(self.id, idx or 0) return self end
            elseif k == "mix" then
                return function(self, val)
                    if type(val) == "table" and val._isGen then
                        _setGen(self.id, "gain", val)
                        _setGen(self.id, "opacity", val)
                    else
                        _set(self.id, "gain", val)
                        _set(self.id, "opacity", val)
                    end
                    return self
                end
            else 
                -- Chainable Setter: s:gain(0.5) returns s
                return function(self, val)
                    if val == nil then return _get(self.id, k) end
                    if type(val) == "table" and val._isGen then _setGen(self.id, k, val)
                    else _set(self.id, k, val) end
                    return self
                end
            end
        end

        _G._allNodes = {}
        _G._autoNames = {}
        _G._nameCount = {}
        
        local function addNodeInternal(t, n, p)
            local nodeName = n
            if not nodeName or nodeName == "" then
                local prefix = (p or t):lower()
                _G._autoNames[prefix] = (_G._autoNames[prefix] or 0) + 1
                nodeName = prefix .. _G._autoNames[prefix]
            else
                -- Dedup: when the same name is reused within one script execution
                -- (e.g. sampler("drums:0") called twice), append a numeric suffix.
                local key = t .. ":" .. n
                _G._nameCount[key] = (_G._nameCount[key] or 0) + 1
                if _G._nameCount[key] > 1 then
                    nodeName = n .. _G._nameCount[key]
                end
            end
            local id = _addNode(t, nodeName)
            if id then
                local node = { id = id, type = t, name = nodeName }
                setmetatable(node, NodeMeta)
                _G._allNodes[id] = node
                _G[nodeName] = node
                return node
            end
            return nil
        end

        function addNode(type, name, params, prefix)
            local nodeName = _G.type(name) == "table" and "" or name
            local nodeParams = _G.type(name) == "table" and name or params
            local node = addNodeInternal(type, nodeName, prefix)
            if node and nodeParams and _G.type(nodeParams) == "table" then
                for k, v in pairs(nodeParams) do
                    node[k] = v
                end
            end
            return node
        end
        
        function clear() 
            for id, node in pairs(_G._allNodes) do
                if type(node) == "table" and node.name then
                    _G[node.name] = nil
                end
            end
            _G._allNodes = {} 
            _G._autoNames = {}
            _G._nameCount = {}
        end

        function getBank(name) return _getBank(name) end
        function playhead(n) return _playhead(n.id or n) end

        function connect(src, dst, paramsOrOut, inIdx)
            if type(src) == "table" and not src.id then
                local lastIdx = 0
                for _, s in ipairs(src) do lastIdx = connect(s, dst, paramsOrOut, inIdx) end
                return lastIdx
            end
            if type(dst) == "table" and not dst.id then
                local lastIdx = 0
                for _, d in ipairs(dst) do lastIdx = connect(src, d, paramsOrOut, inIdx) end
                return lastIdx
            end
            
            local sId = type(src) == "table" and src.id or src
            local dId = type(dst) == "table" and dst.id or dst
            
            if sId and dId then
                local targetIn = inIdx
                local outIdx = 0
                local params = nil

                if type(paramsOrOut) == "table" then
                    params = paramsOrOut
                elseif type(paramsOrOut) == "number" then
                    outIdx = paramsOrOut
                end

                if not targetIn then
                    targetIn = _nextInput(dId)
                end
                
                _connect(sId, dId, outIdx, targetIn)
                
                if params then
                    for pk, pv in pairs(params) do
                        local finalVal = pv
                        if pk == "blend" then
                            if type(pv) == "string" then
                                local upper = pv:upper()
                                if upper == "ALPHA" or upper == "NORMAL" then finalVal = 0
                                elseif upper == "ADD" or upper == "ADDITIVE" then finalVal = 1
                                elseif upper == "MULTIPLY" or upper == "MUL" then finalVal = 2
                                elseif upper == "SCREEN" then finalVal = 3
                                end
                            end
                        end
                        if type(finalVal) == "table" and finalVal._isGen then
                            _setGen(dId, pk .. "_" .. targetIn, finalVal)
                        else
                            _set(dId, pk .. "_" .. targetIn, finalVal)
                        end
                    end
                end
                src._lastTrack = targetIn
                return targetIn
            end
            return nil
        end

        -- Pattern functions
        local function makeGen(t)
            t._isGen = true
            local GenMeta = {
                __mul = function(a, b) local r={type="mul", a=a, b=b}; return makeGen(r) end,
                __add = function(a, b) local r={type="add", a=a, b=b}; return makeGen(r) end,
                __index = {
                    fast = function(self, n) return makeGen({type="fast", n=n, p=self}) end,
                    slow = function(self, n) return makeGen({type="slow", n=n, p=self}) end,
                    shift = function(self, o) return makeGen({type="shift", o=o, p=self}) end,
                    scale = function(self, l, h) return makeGen({type="scale", l=l, h=h, p=self}) end,
                    snap = function(self, s) return makeGen({type="snap", s=s, p=self}) end,
                    abs = function(self) return makeGen({type="abs", p=self}) end,
                    pow = function(self, e) return makeGen({type="pow", e=e or 1.0, p=self}) end,
                    accum = function(self, rate, initial) return makeGen({type="accum", rate=rate or 0.5, init=initial or 0, p=self}) end,
                    smooth = function(self, tau) return makeGen({type="smooth", tau=tau or 1.0, p=self}) end,
                    toggle = function(self, thresh) return makeGen({type="toggle", thresh=thresh or 0.5, p=self}) end
                }
            }
            return setmetatable(t, GenMeta)
        end
        
        _G.makeGen = makeGen
    )";
    lua.doString(helper);
}

Graph* Interpreter::getCurrentGraph() {
    if (s_graphStack.empty()) return nullptr;
    return s_graphStack.back();
}

// Idempotent node creation: reuses existing nodes by type+name (hot-reload survival).
int Interpreter::lua_addNode(lua_State* L) {
    if (!s_currentSession) return 0;
    if (lua_isnil(L, 1)) return 0;
    std::string type = luaL_checkstring(L, 1);
    std::string name = "";
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) name = luaL_checkstring(L, 2);
    Graph* graph = getCurrentGraph();
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
    if (!node) return 0;
    node->touched = true;
    lua_pushinteger(L, node->nodeId);
    return 1;
}

// Runtime node destruction from Lua via node:destroy(). Enqueues REMOVE_NODE for shadow processors.
int Interpreter::lua_removeNode(lua_State* L) {
    if (!s_currentSession) return 0;
    if (lua_isnil(L, 1)) return 0;
    int nodeId = (int)luaL_checkinteger(L, 1);
    Graph* graph = getCurrentGraph();
    if (!graph) return 0;
    graph->removeNode(nodeId);
    return 0;
}

// Find the lowest unused input slot. Skips stale connections so they
// can be re-activated at their original slot during hot-reload.
int Interpreter::lua_nextInput(lua_State* L) {
    if (!s_currentSession) { lua_pushinteger(L, 0); return 1; }
    int nodeId = (int)luaL_checkinteger(L, 1);
    Graph* graph = getCurrentGraph();
    if (!graph) { lua_pushinteger(L, 0); return 1; }
    Node* node = graph->getNode(nodeId);
    if (!node) { lua_pushinteger(L, 0); return 1; }
    std::unordered_set<int> used;
    for (const auto& conn : graph->getConnections()) {
        if (conn.toNode == nodeId && !conn.stale) used.insert(conn.toInput);
    }
    int next = 0;
    while (used.count(next)) next++;
    lua_pushinteger(L, next);
    return 1;
}

// Lua connect() → Graph::connect(). Marks endpoints as touched for hot-reload survival.
int Interpreter::lua_connect(lua_State* L) {
    if (!s_currentSession) return 0;
    if (lua_isnil(L, 1) || lua_isnil(L, 2)) return 0;
    int fromNode = (int)luaL_checkinteger(L, 1);
    int toNode = (int)luaL_checkinteger(L, 2);
    int fromOut = 0, toIn = 0;
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) fromOut = (int)luaL_checkinteger(L, 3);
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) toIn = (int)luaL_checkinteger(L, 4);
    Graph* graph = getCurrentGraph();
    if (auto node = graph->getNode(fromNode)) node->touched = true;
    if (auto node = graph->getNode(toNode)) node->touched = true;
    graph->connect(fromNode, toNode, fromOut, toIn);
    return 0;
}

int Interpreter::lua_getParam(lua_State* L) {
    if (!s_currentSession) return 0;
    int nodeIdx = (int)luaL_checkinteger(L, 1);
    std::string paramName = luaL_checkstring(L, 2);
    Graph* graph = getCurrentGraph();
    Node* node = graph->getNode(nodeIdx);
    if (!node) return 0;
    node->touched = true;
    if (node->parameters->contains(paramName)) {
        auto& p = node->parameters->get(paramName);
        std::string vt = p.valueType();
        if (vt == typeid(float).name()) lua_pushnumber(L, p.cast<float>().get());
        else if (vt == typeid(int).name()) lua_pushinteger(L, p.cast<int>().get());
        else if (vt == typeid(bool).name()) lua_pushboolean(L, p.cast<bool>().get());
        else if (vt == typeid(std::string).name()) lua_pushstring(L, p.cast<std::string>().get().c_str());
        else lua_pushstring(L, p.toString().c_str());
        return 1;
    }
    return 0;
}

static void applyLuaValueToParam(lua_State* L, int stackIdx, Node* node, const std::string& paramName, ofAbstractParameter& p) {
    std::string vt = p.valueType();
    bool isFloat = (vt == "f" || vt == "float" || vt == typeid(float).name());
    bool isInt = (vt == "i" || vt == "int" || vt == typeid(int).name());
    bool isBool = (vt == "b" || vt == "bool" || vt == typeid(bool).name());
    bool isDouble = (vt == "d" || vt == "double" || vt == typeid(double).name());

    if (lua_isboolean(L, stackIdx)) {
        if (isBool) p.cast<bool>().set(lua_toboolean(L, stackIdx));
    } else if (lua_isnumber(L, stackIdx)) {
        double val = lua_tonumber(L, stackIdx);
        if (isInt) p.cast<int>().set((int)val);
        else if (isFloat) p.cast<float>().set((float)val);
        else if (isDouble) p.cast<double>().set(val);
        else if (isBool) p.cast<bool>().set(val > 0.5);
    } else if (lua_isstring(L, stackIdx)) {
        if (vt == typeid(std::string).name() || vt == "string") {
            p.fromString(lua_tostring(L, stackIdx));
        } else {
            auto pat = std::make_shared<patterns::Seq>(lua_tostring(L, stackIdx));
            node->modulate(paramName, pat);
        }
    }
}

int Interpreter::lua_setParam(lua_State* L) {
    if (!s_currentSession) return 0;
    if (lua_isnil(L, 1) || lua_isnil(L, 2)) return 0;
    int nodeIdx = (int)luaL_checkinteger(L, 1);
    std::string paramName = luaL_checkstring(L, 2);
    if (lua_isnil(L, 3)) return 0;
    Graph* graph = getCurrentGraph();
    Node* node = graph ? graph->getNode(nodeIdx) : nullptr;
    if (!node) return 0;
    node->touched = true;

    // 1. Check for proxy parameters (sub-graphs).
    // Sub-graphs delegate parameter changes to their children (e.g. AVSampler children).
    if (auto* g = dynamic_cast<Graph*>(node)) {
        auto targets = g->getProxyTargets(paramName);
        if (!targets.empty()) {
            node->clearModulator(paramName);
            for (auto& [childId, childParam] : targets) {
                Node* child = g->getNode(childId);
                if (!child) continue;
                
                // Clear existing modulator on child to allow static value override
                child->clearModulator(childParam);
                
                if (child->parameters->contains(childParam)) {
                    auto& p = child->parameters->get(childParam);
                    applyLuaValueToParam(L, 3, child, childParam, p);
                    child->onParameterChanged(childParam);
                }
            }
            return 0;
        }
    }

    // 2. Normal parameter update
    node->clearModulator(paramName);
    if (node->parameters->contains(paramName)) {
        auto& p = node->parameters->get(paramName);
        applyLuaValueToParam(L, 3, node, paramName, p);
        node->onParameterChanged(paramName);
    }
    return 0;
}

int Interpreter::lua_clear(lua_State* L) {
    if (!s_currentSession) return 0;
    getCurrentGraph()->clear();
    return 0;
}

int Interpreter::lua_setActive(lua_State* L) {
    if (!s_currentSession) return 0;
    if (lua_isnil(L, 1)) return 0;
    int nodeIdx = (int)luaL_checkinteger(L, 1);
    bool active = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : true;
    Graph* graph = getCurrentGraph();
    Node* node = graph->getNode(nodeIdx);
    if (!node) return 0;
    node->active->set(active);
    node->onParameterChanged("active");
    return 0;
}

int Interpreter::lua_resolvePath(lua_State* L) {
    std::string path = luaL_checkstring(L, 1);
    std::string hint = "";
    if (lua_gettop(L) >= 2) hint = luaL_checkstring(L, 2);
    std::string resolved = AssetRegistry::get().resolve(path, hint);
    if (resolved.empty()) resolved = ConfigManager::get().resolvePath(path);
    lua_pushstring(L, resolved.c_str());
    return 1;
}

int Interpreter::lua_setAlias(lua_State* L) {
    std::string alias = luaL_checkstring(L, 1);
    std::string target = luaL_checkstring(L, 2);
    AssetRegistry::get().registerAlias(alias, target);
    return 0;
}

int Interpreter::lua_getBank(lua_State* L) {
    std::string bankName = luaL_checkstring(L, 1);
    const auto& banks = AssetRegistry::get().getBanks();
    auto it = banks.find(bankName);
    if (it == banks.end()) { lua_newtable(L); return 1; }
    const auto& bank = it->second;
    lua_newtable(L);
    for (size_t i = 0; i < bank.size(); ++i) {
        const auto& asset = bank[i];
        lua_pushinteger(L, i + 1);
        lua_newtable(L);
        lua_pushstring(L, "name"); lua_pushstring(L, asset.name.c_str()); lua_settable(L, -3);
        lua_pushstring(L, "path"); std::string lp = bankName + ":" + std::to_string(i); lua_pushstring(L, lp.c_str()); lua_settable(L, -3);
        lua_pushstring(L, "videoPath"); lua_pushstring(L, asset.videoPath.c_str()); lua_settable(L, -3);
        lua_pushstring(L, "audioPath"); lua_pushstring(L, asset.audioPath.c_str()); lua_settable(L, -3);
        lua_settable(L, -3);
    }
    return 1;
}

std::shared_ptr<Pattern<float>> parsePattern(lua_State* L, int index);

static inline float luaNumField(lua_State* L, int idx, const char* field) {
    lua_getfield(L, idx, field);
    float v = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return v;
}

static inline int luaIntField(lua_State* L, int idx, const char* field) {
    lua_getfield(L, idx, field);
    int v = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return v;
}

static inline std::string luaStrField(lua_State* L, int idx, const char* field) {
    lua_getfield(L, idx, field);
    const char* s = lua_tostring(L, -1);
    std::string result = s ? s : "";
    lua_pop(L, 1);
    return result;
}

static inline std::shared_ptr<Pattern<float>> luaPatField(lua_State* L, int idx, const char* field) {
    lua_getfield(L, idx, field);
    auto p = parsePattern(L, lua_gettop(L));
    lua_pop(L, 1);
    return p;
}

/**
 * Walk a Lua gen table tree and collect unique trigger references
 * (NAME tokens from innermost seq nodes). Recurses through wrapper
 * tables by following "p", "a", and "b" fields.
 *
 * This is the pre-caching mechanism: all refs are known at set time
 * because patterns are stateless and deterministic. Resolving and
 * starting async decode here means the cache hits when triggers fire.
 */
static void collectLuaTriggerRefs(lua_State* L, int index, std::unordered_set<std::string>& out) {
    if (index < 0) index = lua_gettop(L) + index + 1;
    if (lua_type(L, index) != LUA_TTABLE) return;

    lua_getfield(L, index, "type");
    std::string genType = lua_tostring(L, -1) ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);

    if (genType == "seq" || genType == "trigger") {
        lua_getfield(L, index, "val");
        const char* val = lua_tostring(L, -1);
        if (val) {
            std::istringstream ss(val);
            std::string token;
            while (ss >> token) {
                if (token != "~") {
                    out.insert(token);
                }
            }
        }
        lua_pop(L, 1);
        return;
    }

    lua_getfield(L, index, "p");
    if (lua_istable(L, -1)) {
        collectLuaTriggerRefs(L, lua_gettop(L), out);
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "a");
    if (lua_istable(L, -1)) {
        collectLuaTriggerRefs(L, lua_gettop(L), out);
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "b");
    if (lua_istable(L, -1)) {
        collectLuaTriggerRefs(L, lua_gettop(L), out);
    }
    lua_pop(L, 1);
}

int Interpreter::lua_setGenerator(lua_State* L) {
    if (!s_currentSession) return 0;
    int nodeIdx = (int)luaL_checkinteger(L, 1);
    std::string paramName = luaL_checkstring(L, 2);
    Graph* graph = getCurrentGraph();
    Node* node = graph->getNode(nodeIdx);
    if (!node) return 0;
    node->touched = true;
    auto pat = parsePattern(L, 3);
    if (!pat) return 0;

    if (paramName == "path") {
        std::unordered_set<std::string> refs;
        collectLuaTriggerRefs(L, 3, refs);
        if (!refs.empty()) {
            AudioSource* audioSource = nullptr;
            if (auto* g = dynamic_cast<Graph*>(node)) {
                auto targets = g->getProxyTargets(paramName);
                for (auto& [childId, childParam] : targets) {
                    Node* child = g->getNode(childId);
                    if (auto* as = dynamic_cast<AudioSource*>(child)) {
                        audioSource = as;
                        break;
                    }
                }
            } else {
                audioSource = dynamic_cast<AudioSource*>(node);
            }

            if (audioSource) {
                std::string bankName = audioSource->bank.get();
                std::vector<std::string> refVec(refs.begin(), refs.end());

                if (!audioSource->buildTriggerMap(refVec, bankName)) {
                    audioSource->setPendingTriggerBuild(std::move(refVec), bankName);
                }
            }
        }
    }

    if (auto* g = dynamic_cast<Graph*>(node)) {
        auto targets = g->getProxyTargets(paramName);
        if (!targets.empty()) {
            for (auto& [childId, childParam] : targets) {
                Node* child = g->getNode(childId);
                if (child) {
                    child->modulate(childParam, pat);
                    child->onParameterChanged(childParam);
                }
            }
            return 0;
        }
    }

    node->modulate(paramName, pat);
    node->onParameterChanged(paramName);
    return 0;
}

std::shared_ptr<Pattern<float>> parsePattern(lua_State* L, int index) {
    if (index < 0) index = lua_gettop(L) + index + 1;
    int type = lua_type(L, index);
    if (type == LUA_TNUMBER) return std::make_shared<patterns::Constant>((float)lua_tonumber(L, index));
    if (type == LUA_TTABLE) {
        std::string genType = luaStrField(L, index, "type");
        if (genType == "seq" || genType == "trigger") {
            std::string p = luaStrField(L, index, "val");
            std::string bank = luaStrField(L, index, "bank");
            return std::make_shared<patterns::Seq>(p, bank);
        } else if (genType == "osc") {
            float f = luaNumField(L, index, "val");
            return std::make_shared<patterns::Osc>(f);
        } else if (genType == "ramp") {
            float f = luaNumField(L, index, "val");
            return std::make_shared<patterns::Ramp>(f);
        } else if (genType == "noise") {
            float f = luaNumField(L, index, "f");
            float s = luaNumField(L, index, "s");
            return std::make_shared<patterns::Noise>(f, s);
        } else if (genType == "mul") {
            auto pa = luaPatField(L, index, "a");
            auto pb = luaPatField(L, index, "b");
            if (pa && pb) return std::make_shared<patterns::Product>(pa, pb);
        } else if (genType == "add") {
            auto pa = luaPatField(L, index, "a");
            auto pb = luaPatField(L, index, "b");
            if (pa && pb) return std::make_shared<patterns::Sum>(pa, pb);
        } else if (genType == "fast") {
            lua_getfield(L, index, "n");
            std::shared_ptr<Pattern<float>> speedPat = nullptr;
            float n = 1.0f;
            if (lua_istable(L, -1)) {
                speedPat = parsePattern(L, lua_gettop(L));
            } else if (lua_isnumber(L, -1)) {
                n = (float)lua_tonumber(L, -1);
            }
            lua_pop(L, 1);
            auto p = luaPatField(L, index, "p");
            if (p) {
                if (speedPat) return std::make_shared<patterns::Density>(speedPat, p);
                else return std::make_shared<patterns::Density>(n, p);
            }
        } else if (genType == "slow") {
            lua_getfield(L, index, "n");
            std::shared_ptr<Pattern<float>> speedPat = nullptr;
            float n = 1.0f;
            if (lua_istable(L, -1)) {
                speedPat = parsePattern(L, lua_gettop(L));
            } else if (lua_isnumber(L, -1)) {
                n = (float)lua_tonumber(L, -1);
            }
            lua_pop(L, 1);
            auto p = luaPatField(L, index, "p");
            if (p) {
                if (speedPat) {
                    auto invPat = std::make_shared<patterns::Scale>(0.01f, 100.0f, speedPat);
                    return std::make_shared<patterns::Density>(invPat, p);
                } else return std::make_shared<patterns::Density>(1.0f/n, p);
            }
        } else if (genType == "shift") {
            float o = luaNumField(L, index, "o");
            auto p = luaPatField(L, index, "p");
            if (p) return std::make_shared<patterns::Shift>(o, p);
        } else if (genType == "scale") {
            float l = luaNumField(L, index, "l");
            float h = luaNumField(L, index, "h");
            auto p = luaPatField(L, index, "p");
            if (p) return std::make_shared<patterns::Scale>(l, h, p);
        } else if (genType == "snap") {
            float s = luaNumField(L, index, "s");
            auto p = luaPatField(L, index, "p");
            if (p) return std::make_shared<patterns::Snap>(s, p);
        } else if (genType == "abs") {
            auto p = luaPatField(L, index, "p");
            if (p) return std::make_shared<patterns::Abs>(p);
        } else if (genType == "pow") {
            float e = luaNumField(L, index, "e");
            auto p = luaPatField(L, index, "p");
            if (p) return std::make_shared<patterns::Pow>(e, p);
        } else if (genType == "accum") {
            float rate = luaNumField(L, index, "rate");
            float init = luaNumField(L, index, "init");
            auto p = luaPatField(L, index, "p");
            if (p) return std::make_shared<patterns::Accum>(rate, p, init);
        } else if (genType == "smooth") {
            float tau = luaNumField(L, index, "tau");
            auto p = luaPatField(L, index, "p");
            if (p) return std::make_shared<patterns::Smooth>(tau, p);
        } else if (genType == "toggle") {
            float thresh = luaNumField(L, index, "thresh");
            auto p = luaPatField(L, index, "p");
            if (p) return std::make_shared<patterns::Toggle>(p, thresh);
        } else if (genType == "midi") {
            int cc = luaIntField(L, index, "cc");
            int chan = luaIntField(L, index, "chan");
            auto* ptr = Interpreter::s_currentSession->getInputBindings().getMidiBinding(0, chan, cc);
            return std::make_shared<patterns::External>(ptr, "midi:cc:" + std::to_string(chan) + ":" + std::to_string(cc));
        } else if (genType == "midinote") {
            int note = luaIntField(L, index, "note");
            int chan = luaIntField(L, index, "chan");
            auto* ptr = Interpreter::s_currentSession->getInputBindings().getMidiBinding(1, chan, note);
            return std::make_shared<patterns::External>(ptr, "midi:note:" + std::to_string(chan) + ":" + std::to_string(note));
        } else if (genType == "miditouch") {
            int note = luaIntField(L, index, "note");
            int chan = luaIntField(L, index, "chan");
            auto* ptr = Interpreter::s_currentSession->getInputBindings().getMidiBinding(2, chan, note);
            return std::make_shared<patterns::External>(ptr, "midi:touch:" + std::to_string(chan) + ":" + std::to_string(note));
        } else if (genType == "channeltouch") {
            int chan = luaIntField(L, index, "chan");
            std::string path = "midi:ctouch:" + std::to_string(chan);
            auto* ptr = Interpreter::s_currentSession->getInputBindings().getBinding(path);
            return std::make_shared<patterns::External>(ptr, path);
        } else if (genType == "oscin") {
            std::string path = luaStrField(L, index, "path");
            std::string fullPath = "osc:" + path;
            auto* ptr = Interpreter::s_currentSession->getInputBindings().getBinding(fullPath);
            return std::make_shared<patterns::External>(ptr, fullPath);
        } else if (genType == "gamepadbutton") {
            int id = luaIntField(L, index, "id");
            std::string path = "gamepad:button:" + std::to_string(id);
            auto* ptr = Interpreter::s_currentSession->getInputBindings().getBinding(path);
            return std::make_shared<patterns::External>(ptr, path);
        } else if (genType == "gamepadaxis") {
            int id = luaIntField(L, index, "id");
            std::string path = "gamepad:axis:" + std::to_string(id);
            auto* ptr = Interpreter::s_currentSession->getInputBindings().getBinding(path);
            return std::make_shared<patterns::External>(ptr, path);
        }
    }
    return nullptr;
}

int Interpreter::lua_setTempo(lua_State* L) {
    if (!s_currentSession) return 0;
    s_currentSession->getTransport().bpm = (float)luaL_checknumber(L, 1);
    return 0;
}

int Interpreter::lua_playhead(lua_State* L) {
    if (!s_currentSession) { lua_pushnumber(L, 0.0); return 1; }
    int nodeIdx = (int)luaL_checkinteger(L, 1);
    Graph* graph = getCurrentGraph();
    Node* node = graph ? graph->getNode(nodeIdx) : nullptr;
    if (!node) { lua_pushnumber(L, 0.0); return 1; }
    if (auto* g = dynamic_cast<Graph*>(node)) {
        if (auto* resolved = g->resolveAudioOutput(0)) node = resolved;
    }
    if (auto* a = dynamic_cast<AudioSource*>(node)) { lua_pushnumber(L, a->getRelativePosition()); return 1; }
    if (auto* v = dynamic_cast<VideoSource*>(node)) { lua_pushnumber(L, v->getPosition()); return 1; }
    lua_pushnumber(L, 0.0);
    return 1;
}

int Interpreter::lua_outlet(lua_State* L) {
    if (!s_currentSession) return 0;
    int index = 0;
    int nodeId = (int)luaL_checkinteger(L, 1);
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) index = (int)luaL_checkinteger(L, 2);
    Graph* graph = getCurrentGraph();
    if (graph) graph->addOutlet(nodeId, index);
    return 0;
}

int Interpreter::lua_inlet(lua_State* L) {
    if (!s_currentSession) return 0;
    int index = 0;
    int nodeId = (int)luaL_checkinteger(L, 1);
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) index = (int)luaL_checkinteger(L, 2);
    Graph* graph = getCurrentGraph();
    if (graph) graph->addInlet(nodeId, index);
    return 0;
}

int Interpreter::lua_exposeParam(lua_State* L) {
    if (!s_currentSession) return 0;
    if (lua_isnil(L, 1) || lua_isnil(L, 2) || lua_isnil(L, 3)) return 0;
    int childNodeId = (int)luaL_checkinteger(L, 1);
    std::string parentParam = luaL_checkstring(L, 2);
    std::string childParam = luaL_checkstring(L, 3);
    Graph* graph = getCurrentGraph();
    if (graph) {
        graph->addProxyTarget(parentParam, childNodeId, childParam);
    }
    return 0;
}

int Interpreter::lua_readBinding(lua_State* L) {
    if (!s_currentSession) { lua_pushnumber(L, 0.0); return 1; }
    if (!lua_istable(L, 1)) { lua_pushnumber(L, 0.0); return 1; }

    lua_getfield(L, 1, "type");
    if (!lua_isstring(L, -1)) { lua_pop(L, 1); lua_pushnumber(L, 0.0); return 1; }
    std::string genType = lua_tostring(L, -1);
    lua_pop(L, 1);

    std::string path;

    if (genType == "gamepadbutton") {
        lua_getfield(L, 1, "id"); int id = (int)lua_tonumber(L, -1); lua_pop(L, 1);
        path = "gamepad:button:" + std::to_string(id);
    } else if (genType == "gamepadaxis") {
        lua_getfield(L, 1, "id"); int id = (int)lua_tonumber(L, -1); lua_pop(L, 1);
        path = "gamepad:axis:" + std::to_string(id);
    } else if (genType == "midi") {
        lua_getfield(L, 1, "cc"); int cc = (int)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 1, "chan"); int chan = (int)lua_tonumber(L, -1); lua_pop(L, 1);
        auto* ptr = s_currentSession->getInputBindings().getMidiBinding(0, chan, cc);
        float v = ptr ? ptr->load(std::memory_order_acquire) : 0.0f;
        lua_pushnumber(L, v);
        return 1;
    } else if (genType == "midinote") {
        lua_getfield(L, 1, "note"); int note = (int)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 1, "chan"); int chan = (int)lua_tonumber(L, -1); lua_pop(L, 1);
        auto* ptr = s_currentSession->getInputBindings().getMidiBinding(1, chan, note);
        float v = ptr ? ptr->load(std::memory_order_acquire) : 0.0f;
        lua_pushnumber(L, v);
        return 1;
    } else if (genType == "miditouch") {
        lua_getfield(L, 1, "note"); int note = (int)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 1, "chan"); int chan = (int)lua_tonumber(L, -1); lua_pop(L, 1);
        auto* ptr = s_currentSession->getInputBindings().getMidiBinding(2, chan, note);
        float v = ptr ? ptr->load(std::memory_order_acquire) : 0.0f;
        lua_pushnumber(L, v);
        return 1;
    } else if (genType == "channeltouch") {
        lua_getfield(L, 1, "chan"); int chan = (int)lua_tonumber(L, -1); lua_pop(L, 1);
        path = "midi:ctouch:" + std::to_string(chan);
    } else if (genType == "oscin") {
        lua_getfield(L, 1, "path"); std::string p = lua_tostring(L, -1); lua_pop(L, 1);
        path = "osc:" + p;
    } else {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    if (path.empty()) { lua_pushnumber(L, 0.0); return 1; }
    auto* ptr = s_currentSession->getInputBindings().getBinding(path);
    float v = ptr ? ptr->load(std::memory_order_acquire) : 0.0f;
    lua_pushnumber(L, v);
    return 1;
}
