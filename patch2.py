import re

with open("src/core/Graph.cpp", "r") as f:
    data = f.read()

# Replace only processAudio locking logic safely
data = re.sub(
    r"void Graph::processAudio\(ofSoundBuffer& buffer, int index\) \{\n    std::lock_guard<std::recursive_mutex> lock\(audioMutex\);\n",
    """void Graph::processAudio(ofSoundBuffer& buffer, int index) {
    if (!audioMutex.try_lock()) {
        buffer.set(0);
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(audioMutex, std::adopt_lock);\n""", data)

with open("src/core/Graph.cpp", "w") as f:
    f.write(data)
