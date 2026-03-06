import re

with open("src/core/Node.cpp", "r") as f:
    data = f.read()

# Fix the typeid check which is likely failing across translation units
data = data.replace(
    'if (paramBase.type() == typeid(ofParameter<float>).name()) {',
    'if (paramBase.valueType() == typeid(float).name()) {'
)

with open("src/core/Node.cpp", "w") as f:
    f.write(data)
