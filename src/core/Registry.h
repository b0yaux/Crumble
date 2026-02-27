#pragma once
#include "Session.h"

namespace crumble {

/**
 * Registry - Domain-specific bootstrapping for the Crumble application.
 * 
 * In a modular system, the 'Registry' is the bridge between the generic
 * engine (Session/Graph) and the specific node implementations.
 */

// Registers all available node types (VideoMixer, VideoFileSource, etc.)
// with the Session's factory.
void registerNodes(Session& s);

} // namespace crumble
