-- Test alias functionality
clear()

-- Test single alias
alias("t", "travaux")

-- Test multiple aliases
aliases({
    k = "drums:0",
    s = "drums:1",
    test = "travaux"
})

-- Create sampler using alias
local samp = sampler("t")

-- Create sampler with pattern and alias
local drum = s("k", "0 ~ 1 ~ 2")

-- Verify aliases by settingpatterns
samp.n = "0 1" 
drum.n = "test ~ k"

print("Alias test complete - samplers created successfully")