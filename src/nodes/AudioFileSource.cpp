#include "AudioFileSource.h"
#include "../core/Session.h"

AudioFileSource::AudioFileSource() {
    type = "AudioFileSource";

    parameters.add(path.set("path", ""));
    parameters.add(volume.set("volume", 1.0, 0.0, 1.0));
    parameters.add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters.add(loop.set("loop", true));
    parameters.add(playing.set("playing", true));

    path.addListener(this, &AudioFileSource::onPathChanged);
}

void AudioFileSource::pullAudio(ofSoundBuffer& buffer, int index) {
    if (index != 0) return;
    if (!playing || !sharedLoader || !sharedLoader->loaded() || sharedLoader->length() == 0) {
        return;
    }

    float* data = sharedLoader->data();
    size_t totalSamples = sharedLoader->length();
    int channels = sharedLoader->channels();

    for (size_t i = 0; i < buffer.getNumFrames(); i++) {
        size_t frameIndex = (size_t)playhead;

        if (frameIndex < totalSamples) {
            for (int c = 0; c < buffer.getNumChannels(); c++) {
                int sourceChannel = c % channels;
                float sample = data[frameIndex * channels + sourceChannel];
                buffer[i * buffer.getNumChannels() + c] += sample * (float)volume;
            }
        }

        playhead += (float)speed;

        if (playhead >= (float)totalSamples) {
            if (loop) {
                playhead = fmod(playhead, (float)totalSamples);
            } else {
                playing = false;
                playhead = 0;
                break;
            }
        } else if (playhead < 0) {
            if (loop) {
                playhead = (float)totalSamples + fmod(playhead, (float)totalSamples);
            } else {
                playing = false;
                playhead = 0;
                break;
            }
        }
    }
}

std::string AudioFileSource::getDisplayName() const {
    if (path.get().empty()) return "Empty Audio";
    return ofFilePath::getFileName(path.get());
}

ofJson AudioFileSource::serialize() const {
    ofJson j;
    ofSerialize(j, parameters);
    return j;
}

void AudioFileSource::deserialize(const ofJson& json) {
    ofJson j = json;
    // Handle common nesting patterns in our JSON
    if (j.contains("AudioSource")) {
        j = j["AudioSource"];
    } else if (j.contains("group")) {
        j = j["group"];
    } else if (j.contains("params")) {
        j = j["params"];
    }

    // Migrate from old key names (capitalized or legacy) to new lowercase keys
    std::string pathValue;
    if (j.contains("audioPath")) {
        pathValue = getSafeJson<std::string>(j, "audioPath", "");
    } else if (j.contains("path")) {
        pathValue = getSafeJson<std::string>(j, "path", "");
    }
    if (!pathValue.empty()) path.set(pathValue);

    // Handle both old capitalized and new lowercase parameter names
    if (j.contains("Volume")) volume = getSafeJson<float>(j, "Volume", volume.get());
    if (j.contains("volume")) volume = getSafeJson<float>(j, "volume", volume.get());

    if (j.contains("Speed")) speed = getSafeJson<float>(j, "Speed", speed.get());
    if (j.contains("speed")) speed = getSafeJson<float>(j, "speed", speed.get());

    if (j.contains("Loop")) loop = getSafeJson<bool>(j, "Loop", loop.get());
    if (j.contains("loop")) loop = getSafeJson<bool>(j, "loop", loop.get());

    if (j.contains("Playing")) playing = getSafeJson<bool>(j, "Playing", playing.get());
    if (j.contains("playing")) playing = getSafeJson<bool>(j, "playing", playing.get());

    ofDeserialize(j, parameters);
}

void AudioFileSource::onPathChanged(std::string& p) {
    if (p.empty() || p == loadedPath) return;

    if (g_session) {
        sharedLoader = g_session->getAssets().getAudio(p);
        if (sharedLoader) {
            playhead = 0;
            loadedPath = p;
            ofLogNotice("AudioFileSource") << "Got shared audio: " << p;
        } else {
            ofLogError("AudioFileSource") << "FAILED to load: " << p;
        }
    }
}

double AudioFileSource::getRelativePosition() const {
    if (!sharedLoader || !sharedLoader->loaded() || sharedLoader->length() == 0) {
        return 0.0;
    }
    return playhead / (double)sharedLoader->length();
}
