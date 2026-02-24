#include "Inlet.h"

Inlet::Inlet() {
    type = "Inlet";
    name = "Inlet";
}

ofTexture* Inlet::getVideoOutput() {
    return nullptr;
}

std::string Inlet::getDisplayName() const {
    return "In " + std::to_string(inletIndex);
}
