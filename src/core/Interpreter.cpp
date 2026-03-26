#include "Interpreter.h"
#include "Config.h"
#include "AssetRegistry.h"
#include "PatternMath.h"

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
    lua.doString("_autoIndices = {}; _autoNames = {}");
    session->getGraph().markConnectionsStale();
    
    bool success = lua.doScript(path, true);
    
    // Clean up nodes that weren't touched (not re-created in script)
    session->endScript();
    session->getGraph().pruneStaleConnections();
    
    s_currentSession = nullptr;
    return success;
}

bool Interpreter::runScriptInGraph(const std::string& path, Graph* nestedGraph) {
    if (!session || !nestedGraph) return false;
    
    s_currentSession = session;
    GraphContext ctx(nestedGraph);
    
    // Reset auto-indexing for nested graph execution
    lua.doString("_autoIndices = {}; _autoNames = {}");
    
    bool success = lua.doScript(path, true);
    
    s_currentSession = nullptr;
    return success;
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
    lua_register(L, "_connect", lua_connect);
    lua_register(L, "_get", lua_getParam);
    lua_register(L, "_set", lua_setParam);
    lua_register(L, "_setGen", lua_setGenerator);
    lua_register(L, "_setActive", lua_setActive);
    lua_register(L, "_clear", lua_clear);
    lua_register(L, "_resolve", lua_resolvePath);
    lua_register(L, "_getBank", lua_getBank);
    lua_register(L, "_setTempo", lua_setTempo);
    lua_register(L, "_midi", lua_midi);
    lua_register(L, "_oscin", lua_oscin);
    
    std::string helper = R"(
        session = {}
        local NodeMeta = {}
        
        function bpm(v) _setTempo(v) end
        function cpm(v) _setTempo(v * 4) end
        function cps(v) _setTempo(v * 240) end

        
        function NodeMeta:__newindex(key, value)
            if key == "id" or key == "type" or key == "name" then
                rawset(self, key, value)
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
                return function(self, toNode, fromOut, toIn)
                    local toId = type(toNode) == "table" and toNode.id or toNode
                    _connect(self.id, toId, fromOut or 0, toIn or 0)
                    return self
                end
            elseif k == "off" then return function(self) _setActive(self.id, false) return self end
            elseif k == "on" then return function(self) _setActive(self.id, true) return self end
            elseif k == "mute" then return function(self) _setActive(self.id, false) return self end
            elseif k == "unmute" then return function(self) _setActive(self.id, true) return self end
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
        _G._autoIndices = {}
        
        function getBank(name) return _getBank(name) end
        
        _G._autoNames = {}
        local function addNodeInternal(t, n, p)
            local nodeName = n
            if not nodeName or nodeName == "" then
                local prefix = (p or t):lower()
                _G._autoNames[prefix] = (_G._autoNames[prefix] or 0) + 1
                nodeName = prefix .. _G._autoNames[prefix]
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
            _clear() 
            _G._allNodes = {} 
            _G._autoIndices = {} 
            _G._autoNames = {}
        end

        function connect(src, dst, outIdx, inIdx)
            if type(src) == "table" and not src.id then
                local lastIdx = 0
                for _, s in ipairs(src) do lastIdx = connect(s, dst, outIdx, inIdx) end
                return lastIdx
            end
            if type(dst) == "table" and not dst.id then
                local lastIdx = 0
                for _, d in ipairs(dst) do lastIdx = connect(src, d, outIdx, inIdx) end
                return lastIdx
            end
            
            local sId = type(src) == "table" and src.id or src
            local dId = type(dst) == "table" and dst.id or dst
            
            if sId and dId then
                local targetIn = inIdx
                if not targetIn then
                    _G._autoIndices[dId] = (_G._autoIndices[dId] or -1) + 1
                    targetIn = _G._autoIndices[dId]
                end
                _connect(sId, dId, outIdx or 0, targetIn)
                return targetIn
            end
            return -1
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
                    abs = function(self) return makeGen({type="abs", p=self}) end
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

int Interpreter::lua_addNode(lua_State* L) {
    if (!s_currentSession) return 0;
    if (lua_isnil(L, 1)) {
        ofLogWarning("Interpreter") << "addNode: type argument is nil, ignoring";
        return 0;
    }
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
    if (!node) {
        ofLogWarning("Interpreter") << "addNode: failed to create node of type '" << type << "'";
        return 0;
    }
    lua_pushinteger(L, node->nodeId);
    return 1;
}

int Interpreter::lua_connect(lua_State* L) {
    if (!s_currentSession) return 0;
    if (lua_isnil(L, 1) || lua_isnil(L, 2)) {
        ofLogWarning("Interpreter") << "connect: source or destination node ID is nil, ignoring";
        return 0;
    }
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
        if (p.type() == typeid(ofParameter<float>).name()) lua_pushnumber(L, p.cast<float>().get());
        else if (p.type() == typeid(ofParameter<int>).name()) lua_pushinteger(L, p.cast<int>().get());
        else if (p.type() == typeid(ofParameter<bool>).name()) lua_pushboolean(L, p.cast<bool>().get());
        else if (p.type() == typeid(ofParameter<std::string>).name()) lua_pushstring(L, p.cast<std::string>().get().c_str());
        else lua_pushstring(L, p.toString().c_str());
        return 1;
    }
    return 0;
}

int Interpreter::lua_setParam(lua_State* L) {
    if (!s_currentSession) return 0;
    if (lua_isnil(L, 1) || lua_isnil(L, 2)) return 0;
    int nodeIdx = (int)luaL_checkinteger(L, 1);
    std::string paramName = luaL_checkstring(L, 2);
    if (lua_isnil(L, 3)) {
        ofLogWarning("Interpreter") << "lua_setParam: tried to set '" << paramName << "' to nil on node " << nodeIdx << ". Ignoring.";
        return 0;
    }
    Graph* graph = getCurrentGraph();
    Node* node = graph->getNode(nodeIdx);
    if (!node) return 0;
    node->touched = true;
    node->clearModulator(paramName);
    if (node->parameters->contains(paramName)) {
        auto& p = node->parameters->get(paramName);
        if (lua_isboolean(L, 3)) p.cast<bool>().set(lua_toboolean(L, 3));
        else if (lua_isnumber(L, 3)) {
            double val = lua_tonumber(L, 3);
            if (p.type() == typeid(ofParameter<int>).name()) p.cast<int>().set((int)val);
            else if (p.type() == typeid(ofParameter<float>).name()) p.cast<float>().set((float)val);
            else if (p.type() == typeid(ofParameter<double>).name()) p.cast<double>().set(val);
            else if (p.type() == typeid(ofParameter<bool>).name()) p.cast<bool>().set(val > 0.5);
        } else if (lua_isstring(L, 3)) p.fromString(lua_tostring(L, 3));
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

int Interpreter::lua_listDirectory(lua_State* L) {
    std::string path = luaL_checkstring(L, 1);
    ofDirectory dir(path);
    dir.listDir();
    lua_newtable(L);
    for (size_t i = 0; i < dir.size(); ++i) {
        lua_pushinteger(L, i + 1);
        lua_pushstring(L, dir.getPath(i).c_str());
        lua_settable(L, -3);
    }
    return 1;
}

int Interpreter::lua_fileExists(lua_State* L) {
    std::string path = luaL_checkstring(L, 1);
    lua_pushboolean(L, ofFile(path).exists());
    return 1;
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

int Interpreter::lua_setGenerator(lua_State* L) {
    if (!s_currentSession) return 0;
    int nodeIdx = (int)luaL_checkinteger(L, 1);
    std::string paramName = luaL_checkstring(L, 2);
    Graph* graph = getCurrentGraph();
    Node* node = graph->getNode(nodeIdx);
    if (!node) {
        ofLogWarning("Interpreter") << "lua_setGenerator: node not found " << nodeIdx;
        return 0;
    }
    node->touched = true;
    auto pat = parsePattern(L, 3);
    ofLogNotice("Interpreter") << "lua_setGenerator: node=" << node->name << " param=" << paramName << " pattern=" << (pat ? pat->getSignature() : "null");
    if (pat) {
        node->modulate(paramName, pat);
        node->onParameterChanged(paramName);  // Force pattern to be sent to audio thread
    }
    return 0;
}

std::shared_ptr<Pattern<float>> parsePattern(lua_State* L, int index) {
    if (index < 0) index = lua_gettop(L) + index + 1;
    int type = lua_type(L, index);
    if (type == LUA_TNUMBER) return std::make_shared<patterns::Constant>((float)lua_tonumber(L, index));
    if (type == LUA_TTABLE) {
        lua_getfield(L, index, "type");
        std::string genType = lua_tostring(L, -1) ? lua_tostring(L, -1) : "";
        lua_pop(L, 1);
        if (genType == "seq") {
            lua_getfield(L, index, "val");
            std::string p = lua_tostring(L, -1) ? lua_tostring(L, -1) : "";
            lua_pop(L, 1); return std::make_shared<patterns::Seq>(p);
        } else if (genType == "osc") {
            lua_getfield(L, index, "val");
            float f = (float)lua_tonumber(L, -1);
            lua_pop(L, 1); return std::make_shared<patterns::Osc>(f);
        } else if (genType == "ramp") {
            lua_getfield(L, index, "val");
            float f = (float)lua_tonumber(L, -1);
            lua_pop(L, 1); return std::make_shared<patterns::Ramp>(f);
        } else if (genType == "noise") {
            lua_getfield(L, index, "f");
            float f = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
            lua_getfield(L, index, "s");
            float s = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
            return std::make_shared<patterns::Noise>(f, s);
        } else if (genType == "mul") {
            lua_getfield(L, index, "a"); auto pa = parsePattern(L, lua_gettop(L)); lua_pop(L, 1);
            lua_getfield(L, index, "b"); auto pb = parsePattern(L, lua_gettop(L)); lua_pop(L, 1);
            if (pa && pb) return std::make_shared<patterns::Product>(pa, pb);
        } else if (genType == "add") {
            lua_getfield(L, index, "a"); auto pa = parsePattern(L, lua_gettop(L)); lua_pop(L, 1);
            lua_getfield(L, index, "b"); auto pb = parsePattern(L, lua_gettop(L)); lua_pop(L, 1);
            if (pa && pb) return std::make_shared<patterns::Sum>(pa, pb);
        } else if (genType == "fast") {
            lua_getfield(L, index, "n"); float n = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, index, "p"); auto p = parsePattern(L, lua_gettop(L)); lua_pop(L, 1);
            if (p) return std::make_shared<patterns::Speed>(n, p);
        } else if (genType == "slow") {
            lua_getfield(L, index, "n"); float n = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, index, "p"); auto p = parsePattern(L, lua_gettop(L)); lua_pop(L, 1);
            if (p) return std::make_shared<patterns::Speed>(1.0f/n, p);
        } else if (genType == "shift") {
            lua_getfield(L, index, "o"); float o = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, index, "p"); auto p = parsePattern(L, lua_gettop(L)); lua_pop(L, 1);
            if (p) return std::make_shared<patterns::Shift>(o, p);
        } else if (genType == "scale") {
            lua_getfield(L, index, "l"); float l = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, index, "h"); float h = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, index, "p"); auto p = parsePattern(L, lua_gettop(L)); lua_pop(L, 1);
            if (p) return std::make_shared<patterns::Scale>(l, h, p);
        } else if (genType == "snap") {
            lua_getfield(L, index, "s"); float s = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, index, "p"); auto p = parsePattern(L, lua_gettop(L)); lua_pop(L, 1);
            if (p) return std::make_shared<patterns::Snap>(s, p);
        } else if (genType == "abs") {
            lua_getfield(L, index, "p"); auto p = parsePattern(L, lua_gettop(L)); lua_pop(L, 1);
            if (p) return std::make_shared<patterns::Abs>(p);
        } else if (genType == "midi") {
            lua_getfield(L, index, "cc"); int cc = (int)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, index, "chan"); int chan = (int)lua_tonumber(L, -1); lua_pop(L, 1);
            auto* ptr = Interpreter::s_currentSession->getInputManager().getMidiBinding(0, chan, cc);
            return std::make_shared<patterns::External>(ptr, "midi:cc:" + std::to_string(chan) + ":" + std::to_string(cc));
        } else if (genType == "midinote") {
            lua_getfield(L, index, "note"); int note = (int)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, index, "chan"); int chan = (int)lua_tonumber(L, -1); lua_pop(L, 1);
            auto* ptr = Interpreter::s_currentSession->getInputManager().getMidiBinding(1, chan, note);
            return std::make_shared<patterns::External>(ptr, "midi:note:" + std::to_string(chan) + ":" + std::to_string(note));
        } else if (genType == "miditouch") {
            lua_getfield(L, index, "note"); int note = (int)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, index, "chan"); int chan = (int)lua_tonumber(L, -1); lua_pop(L, 1);
            auto* ptr = Interpreter::s_currentSession->getInputManager().getMidiBinding(2, chan, note);
            return std::make_shared<patterns::External>(ptr, "midi:touch:" + std::to_string(chan) + ":" + std::to_string(note));
        } else if (genType == "channeltouch") {
            lua_getfield(L, index, "chan"); int chan = (int)lua_tonumber(L, -1); lua_pop(L, 1);
            std::string path = "midi:ctouch:" + std::to_string(chan);
            auto* ptr = Interpreter::s_currentSession->getInputManager().getBinding(path);
            return std::make_shared<patterns::External>(ptr, path);
        } else if (genType == "oscin") {
            lua_getfield(L, index, "path"); std::string path = lua_tostring(L, -1); lua_pop(L, 1);
            std::string fullPath = "osc:" + path;
            auto* ptr = Interpreter::s_currentSession->getInputManager().getBinding(fullPath);
            return std::make_shared<patterns::External>(ptr, fullPath);
        } else if (genType == "gamepadbutton") {
            lua_getfield(L, index, "id"); int id = (int)lua_tonumber(L, -1); lua_pop(L, 1);
            std::string path = "gamepad:button:" + std::to_string(id);
            auto* ptr = Interpreter::s_currentSession->getInputManager().getBinding(path);
            return std::make_shared<patterns::External>(ptr, path);
        } else if (genType == "gamepadaxis") {
            lua_getfield(L, index, "id"); int id = (int)lua_tonumber(L, -1); lua_pop(L, 1);
            std::string path = "gamepad:axis:" + std::to_string(id);
            auto* ptr = Interpreter::s_currentSession->getInputManager().getBinding(path);
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

int Interpreter::lua_midi(lua_State* L) { return 0; }
int Interpreter::lua_oscin(lua_State* L) { return 0; }
