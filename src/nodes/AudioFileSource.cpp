#include "AudioFileSource.h"
#include "../core/Session.h"

AudioFileSource::AudioFileSource() {
    type = "AudioFileSource";

    parameters.add(path.set("path", ""));
    parameters.add(speed.set("speed", 1.0, -4.0, 4.0));
    parameters.add(loop.set("loop", true));
    parameters.add(playing.set("playing", true));

    path.addListener(this, &AudioFileSource::onPathChanged);
}

void AudioFileSource::processAudio(ofSoundBuffer& buffer, int index) {
    if (index != 0) return;
    if (!playing || !sharedLoader || !sharedLoader->loaded() || sharedLoader->length() == 0) {
        return;
    }

    float* data = sharedLoader->data();
    size_t totalSamples = sharedLoader->length();
    int channels = sharedLoader->channels();

    // 1. Get pre-calculated control streams (Pushed by the engine)
    Control speedCtrl = getControl(speed);

    for (size_t i = 0; i < buffer.getNumFrames(); i++) {
        double currentPlayhead = playhead.load();
        size_t frameIndex = (size_t)currentPlayhead;

        if (frameIndex < totalSamples) {
            for (int c = 0; c < buffer.getNumChannels(); c++) {
                int sourceChannel = c % channels;
                float sample = data[frameIndex * channels + sourceChannel];
                buffer[i * buffer.getNumChannels() + c] += sample;
            }
        }

        currentPlayhead += (double)speedCtrl[i];

        if (currentPlayhead >= (double)totalSamples) {
            if (loop) {
                currentPlayhead = fmod(currentPlayhead, (double)totalSamples);
            } else {
                playing = false;
                currentPlayhead = 0;
            }
        } else if (currentPlayhead < 0) {
            if (loop) {
                currentPlayhead = (double)totalSamples + fmod(currentPlayhead, (double)totalSamples);
            } else {
                playing = false;
                currentPlayhead = 0;
            }
        }
        
        playhead.store(currentPlayhead);
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
    if (j.contains("AudioSource")) j = j["AudioSource"];
    else if (j.contains("group")) j = j["group"];
    else if (j.contains("params")) j = j["params"];

    std::string pathValue;
    if (j.contains("audioPath")) pathValue = getSafeJson<std::string>(j, "audioPath", "");
    else if (j.contains("path")) pathValue = getSafeJson<std::string>(j, "path", "");
    if (!pathValue.empty()) path.set(pathValue);

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
    if (!p.empty() && p != loadedPath) {
        load(p);
    }
}

void AudioFileSource::load(const std::string& p) {
    path = p;
    
    if (g_session) {
        sharedLoader = g_session->getCache().getAudio(p);
        if (sharedLoader) {
            playhead.store(0.0);
            loadedPath = p;
            ofLogNotice("AudioFileSource") << "Got shared audio: " << p;
        } else {
            ofLogError("AudioFileSource") << "FAILED to load: " << p;
        }
    }
}

double AudioFileSource::getRelativePosition() const {
    if (!sharedLoader || !sharedLoader->loaded() || sharedLoader->length() == 0) return 0.0;
    return playhead.load() / (double)sharedLoader->length();
}

void AudioFileSource::setRelativePosition(double pct) {
    if (!sharedLoader || !sharedLoader->loaded() || sharedLoader->length() == 0) return;
    playhead.store(ofClamp(pct, 0.0, 1.0) * (double)sharedLoader->length());
}
