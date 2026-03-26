#include <vector>
#include "InputManager.h"
#include "ofLog.h"
#include "ofxMidi.h"
#include "ofxOsc.h"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

namespace crumble {

// Internal implementation class to hide ofxMidi/ofxOsc/SDL headers from the public API
class InputManagerImpl : public ofxMidiListener {
public:
    void setup(InputManager* owner) {
        this->owner = owner;
        
        // MIDI Setup - Open all available ports
        std::vector<std::string> ports = midiInHelper.getInPortList();
        for (int i = 0; i < (int)ports.size(); i++) {
            auto sub = std::make_unique<ofxMidiIn>();
            sub->openPort(i);
            sub->addListener(this);
            ofLogNotice("InputManager") << "MIDI input opened on port " << i << ": " << ports[i];
            midiInputs.push_back(std::move(sub));
        }

        // OSC Setup
        oscReceiver.setup(8000);
        ofLogNotice("InputManager") << "OSC receiver started on port 8000";

        // Gamepad Setup
        setupGamepad();
    }

    void setupGamepad() {
        if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
            ofLogError("InputManager") << "SDL_Init Error: " << SDL_GetError();
            sdlInitialized = false;
            return;
        }
        sdlInitialized = true;
        
        // Open the first available controller
        if (SDL_NumJoysticks() > 0) {
            for (int i = 0; i < SDL_NumJoysticks(); i++) {
                if (SDL_IsGameController(i)) {
                    openGamepad(i);
                    break;
                }
            }
        }
        
        // Initialize axis/button state arrays
        axisValues.fill(0.0f);
        buttonStates.fill(0.0f);
    }

    void openGamepad(int deviceIndex) {
        closeGamepad();
        controller = SDL_GameControllerOpen(deviceIndex);
        if (controller) {
            ofLogNotice("InputManager") << "Gamepad connected: " << SDL_GameControllerName(controller);
        }
    }

    void closeGamepad() {
        if (controller) {
            SDL_GameControllerClose(controller);
            controller = nullptr;
            ofLogNotice("InputManager") << "Gamepad disconnected";
        }
    }

    void exit() {
        closeGamepad();
        if (sdlInitialized) {
            SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
            sdlInitialized = false;
        }
    }

    void update() {
        // OSC messages
        while (oscReceiver.hasWaitingMessages()) {
            ofxOscMessage m;
            oscReceiver.getNextMessage(m);
            owner->setBinding("osc:" + m.getAddress(), m.getArgAsFloat(0));
        }

        // SDL Gamepad events
        if (!sdlInitialized) return;
        
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_CONTROLLERBUTTONUP: {
                    int btn = e.cbutton.button;
                    float value = (e.type == SDL_CONTROLLERBUTTONDOWN) ? 1.0f : 0.0f;
                    if (btn >= 0 && btn < SDL_CONTROLLER_BUTTON_MAX) {
                        buttonStates[btn] = value;
                        std::string path = "gamepad:button:" + std::to_string(btn);
                        owner->setBinding(path, value);
                    }
                    break;
                }
                
                case SDL_CONTROLLERAXISMOTION: {
                    int axis = e.caxis.axis;
                    if (axis >= 0 && axis < SDL_CONTROLLER_AXIS_MAX) {
                        float value = e.caxis.value / 32767.0f;
                        // Apply deadzone
                        if (fabsf(value) < 0.18f) value = 0.0f;
                        axisValues[axis] = value;
                        std::string path = "gamepad:axis:" + std::to_string(axis);
                        owner->setBinding(path, value);
                    }
                    break;
                }
                
                case SDL_CONTROLLERDEVICEADDED: {
                    if (!controller && SDL_IsGameController(e.cdevice.which)) {
                        openGamepad(e.cdevice.which);
                    }
                    break;
                }
                
                case SDL_CONTROLLERDEVICEREMOVED: {
                    SDL_GameController* removed = SDL_GameControllerFromInstanceID(e.cdevice.which);
                    if (removed == controller) {
                        closeGamepad();
                    }
                    break;
                }
            }
        }
    }

    // ofxMidiListener callback
    void newMidiMessage(ofxMidiMessage& msg) override {
        if (msg.status == MIDI_CONTROL_CHANGE) {
            std::string path = "midi:cc:" + std::to_string(msg.channel) + ":" + std::to_string(msg.control);
            owner->setBinding(path, msg.value / 127.0f);
            ofLog(OF_LOG_NOTICE, "[MIDI] cc(%d, %d) -> %.2f  [%s]", msg.control, msg.channel, msg.value / 127.0f, path.c_str());
        } 
        else if (msg.status == MIDI_NOTE_ON) {
            std::string path = "midi:note:" + std::to_string(msg.channel) + ":" + std::to_string(msg.pitch);
            owner->setBinding(path, msg.velocity / 127.0f);
            ofLog(OF_LOG_NOTICE, "[MIDI] midinote(%d, %d) -> %.2f [%s]", msg.pitch, msg.channel, msg.velocity / 127.0f, path.c_str());
        }
        else if (msg.status == MIDI_NOTE_OFF) {
            std::string path = "midi:note:" + std::to_string(msg.channel) + ":" + std::to_string(msg.pitch);
            owner->setBinding(path, 0.0f);
            ofLog(OF_LOG_NOTICE, "[MIDI] midinote(%d, %d) -> OFF  [%s]", msg.pitch, msg.channel, path.c_str());
        }
        else if (msg.status == MIDI_POLY_AFTERTOUCH) {
            std::string path = "midi:touch:" + std::to_string(msg.channel) + ":" + std::to_string(msg.pitch);
            owner->setBinding(path, msg.value / 127.0f);
            ofLog(OF_LOG_NOTICE, "[MIDI] miditouch(%d, %d) -> %.2f [%s]", msg.pitch, msg.channel, msg.value / 127.0f, path.c_str());
        }
        else if (msg.status == MIDI_AFTERTOUCH) {
            std::string path = "midi:ctouch:" + std::to_string(msg.channel);
            owner->setBinding(path, msg.value / 127.0f);
            ofLog(OF_LOG_NOTICE, "[MIDI] channeltouch(%d) -> %.2f [%s]", msg.channel, msg.value / 127.0f, path.c_str());
        }
    }

private:
    InputManager* owner = nullptr;
    ofxMidiIn midiInHelper;
    std::vector<std::unique_ptr<ofxMidiIn>> midiInputs;
    ofxOscReceiver oscReceiver;
    
    // SDL Gamepad state
    SDL_GameController* controller = nullptr;
    bool sdlInitialized = false;
    std::array<float, SDL_CONTROLLER_AXIS_MAX> axisValues{};
    std::array<float, SDL_CONTROLLER_BUTTON_MAX> buttonStates{};
};

InputManager::InputManager() : impl(std::make_unique<InputManagerImpl>()) {
    for (auto& val : midiStore) val.store(0.0f);
}
InputManager::~InputManager() = default;

int InputManager::getMidiIndex(int statusOffset, int chan, int num) {
    // statusOffset: 0=CC, 1=Note, 2=Touch
    // Clamp to valid MIDI ranges
    int c = std::max(1, std::min(16, chan)) - 1;
    int n = std::max(0, std::min(127, num));
    return (statusOffset * 16 * 128) + (c * 128) + n;
}

std::atomic<float>* InputManager::getMidiBinding(int statusOffset, int chan, int num) {
    return &midiStore[getMidiIndex(statusOffset, chan, num)];
}

std::atomic<float>* InputManager::getBinding(const std::string& path) {
    // Fast path for MIDI
    if (path.rfind("midi:", 0) == 0) {
        int status = -1;
        if (path.find("midi:cc:") == 0) status = 0;
        else if (path.find("midi:note:") == 0) status = 1;
        else if (path.find("midi:touch:") == 0) status = 2;

        if (status != -1) {
            size_t lastColon = path.find_last_of(':');
            size_t midColon = path.find_last_of(':', lastColon - 1);
            if (lastColon != std::string::npos && midColon != std::string::npos) {
                int chan = std::stoi(path.substr(midColon + 1, lastColon - midColon - 1));
                int num = std::stoi(path.substr(lastColon + 1));
                return getMidiBinding(status, chan, num);
            }
        }
    }

    // Standard map path (OSC/Gamepad)
    std::lock_guard<std::mutex> lock(mutex);
    if (namedStore.find(path) == namedStore.end()) {
        namedStore[path] = std::make_unique<std::atomic<float>>(0.0f);
    }
    return namedStore[path].get();
}

void InputManager::setBinding(const std::string& path, float value) {
    auto* ptr = getBinding(path);
    if (ptr) ptr->store(value, std::memory_order_release);
}

void InputManager::setup() {
    impl->setup(this);
}

void InputManager::update() {
    impl->update();
}

} // namespace crumble
