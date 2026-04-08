-- noise.lua — people screaming into it

bpm(140)

local screen = videoout("screen"):on()
local vmix = videomix("vmix"):connect(screen):on()
local speakers = audioout("speakers"):on()
local amix = audiomix("amix"):connect(speakers):on():gain(0.7)

local avmix = {vmix, amix}

-- the chaos: pick random indices, chop fast, layer thick
-- bank is 120 clips, we grab slices and slam them

-- rhythm base — stuttered triggers on the bank
local k = sampler("superstratum_video-data:0"):connect(avmix):on()
    :path(seq("0 [0 0] ~ 0"):fast(2))
    :speed(noise(2):scale(0.5, 2))
    :gain(0.8)
    :blend(1)

local s1 = sampler("superstratum_video-data:23"):connect(avmix):on()
    :path(seq("~ 1 ~ [1 1]"):fast(2))
    :speed(saw(8):scale(-2, 2):abs())
    :gain(noise(4):scale(0.3, 1))
    :blend(1)

local s2 = sampler("superstratum_video-data:47"):connect(avmix):on()
    :path(seq("~ ~ 2 ~")):fast(1)
    :speed(noise(1):scale(0.25, 4))
    :gain(0.9)
    :blend(1)

local s3 = sampler("superstratum_video-data:89"):connect(avmix):on()
    :path(seq("~ ~ ~ [3 3 3]"):fast(3))
    :speed(noise(8):scale(-1, 3))
    :gain(noise(2):scale(0.2, 0.8))
    :blend(0)

-- the drone: slow, thick, additive blend
local drone = sampler("superstratum_video-data:60"):connect(avmix):on()
    :path(seq("0"):slow(8))
    :speed(sine(0.05):scale(0.1, 0.8))
    :gain(0.5)
    :blend(1)

-- nous material — personal noise
local nous = sampler("Noûs-performance-ka:0"):connect(avmix):on()
    :path(seq("0"):slow(4))
    :speed(noise(0.5):scale(0.3, 1.5))
    :gain(0.4)
    :blend(1)

-- drums on top if they resolve
local dk = sampler("drums:0"):connect(avmix):on()
    :path(seq("0 [~ 0] 0 [0 ~ 0]"):fast(2))
    :gain(noise(4):scale(0.3, 1))
    :blend(0)

-- layer gains — everything fights for space
amix.gain_1 = noise(2):scale(0.3, 0.9)
amix.gain_2 = sine(0.25):scale(0.2, 0.8)
amix.gain_3 = noise(1):scale(0.1, 0.6)
amix.gain_4 = sine(0.5):scale(0.1, 0.5)
amix.gain_5 = noise(0.25):scale(0.2, 0.7)
amix.gain_6 = sine(0.1):scale(0.1, 0.4)
amix.gain_7 = 0.6

-- video: additive chaos, opacity surging
vmix.opacity_1 = noise(3):scale(0.2, 1)
vmix.opacity_2 = sine(1):scale(0.1, 0.9)
vmix.opacity_3 = noise(2):scale(0.3, 1)
vmix.opacity_4 = saw(0.5):scale(0.1, 0.7)
vmix.opacity_5 = noise(0.5):scale(0.3, 0.8)
vmix.opacity_6 = sine(0.25):scale(0.2, 0.6)
vmix.opacity_7 = noise(1):scale(0.1, 0.5)

print("NOISE. 7 samplers. screaming.")
