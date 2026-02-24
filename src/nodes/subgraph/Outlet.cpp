#include "Outlet.h"

Outlet::Outlet() {
    type = "Outlet";
    name = "Outlet";
}

ofTexture* Outlet::getVideoOutput() {
    return nullptr;
}

std::string Outlet::getDisplayName() const {
    return "Out " + std::to_string(outletIndex);
}
