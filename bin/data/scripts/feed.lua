clear() 
local ao = aout():on()
local vo = vout():on()
local amix = amix():connect(ao)
local vmix = vmix():connect(vo)
local avmix = {vmix,amix}

local f1 = sampler("feed"):connect(avmix):on():blend(0):opacity(1)
    :path(seq("1 2"))

local f2 = sampler("feed"):connect(avmix):on():blend(2)
    :path(seq("9 8"):fast(seq("10 6 2 <~ 1>")))