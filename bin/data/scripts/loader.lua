-- loader.lua
-- Generic directory scanner
-- Returns: { ".mov" = {{name, path, ext}}, ".wav" = {{name, path, ext}}, ... }

local M = {}

function M.scan(dir, options)
    options = options or {}
    local extensions = options.extensions or {".mov", ".mp4", ".hap", ".avi", ".wav", ".mp3", ".aif"}
    local limit = options.limit or math.huge
    
    local files = _listDir(dir)
    local byExt = {}
    
    for _, path in ipairs(files) do
        local ext = path:lower():match("(%.[^.]+)$")
        if ext then
            -- Check if extension is allowed
            local allowed = false
            for _, e in ipairs(extensions) do
                if ext == e:lower() then
                    allowed = true
                    break
                end
            end
            
            if allowed then
                local name = path:match("([^/]+)%.[^.]+$")
                local entry = { name = name, path = path, ext = ext }
                
                if not byExt[ext] then byExt[ext] = {} end
                table.insert(byExt[ext], entry)
                
                local total = 0
                for _, list in pairs(byExt) do total = total + #list end
                if total >= limit then break end
            end
        end
    end
    
    return byExt
end

return M
