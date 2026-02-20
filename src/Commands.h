#pragma once
#include "core/Graph.h"
#include <functional>
#include <vector>
#include <variant>

// Minimal command system for undo/redo of whole-graph operations.
// Single-node ops (opacity, blend, etc.) don't go through commands yet.
// Compound ops (addVideoLayer, removeLayer) bypass undo/redo for now.

namespace cmd {

// Load a patch file
struct LoadPatch {
    std::string filePath;
    ofJson previousState;  // Save current state for undo

    void execute(Graph& g) {
        // Save current state
        previousState = g.toJson();

        // Load new state
        g.loadFromFile(filePath);
    }

    void undo(Graph& g) {
        if (!previousState.empty()) {
            g.fromJson(previousState);
        }
    }
};

// Clear the entire graph
struct Clear {
    ofJson previousState;

    void execute(Graph& g) {
        previousState = g.toJson();
        g.clear();
    }

    void undo(Graph& g) {
        if (!previousState.empty()) {
            g.fromJson(previousState);
        }
    }
};

// Variant type to hold any command
using Command = std::variant<LoadPatch, Clear>;

} // namespace cmd

// Simple command history with undo/redo
class CommandHistory {
public:
    template<typename Cmd>
    void execute(Cmd cmd, Graph& g) {
        cmd.execute(g);

        // Truncate redo history
        if (currentPos < (int)history.size() - 1) {
            history.erase(history.begin() + currentPos + 1, history.end());
        }

        // Add to history
        history.push_back(std::move(cmd));
        currentPos++;

        // Limit history size
        if (history.size() > maxHistorySize) {
            history.erase(history.begin());
            currentPos--;
        }
    }

    void undo(Graph& g) {
        if (currentPos >= 0) {
            std::visit([&g](auto& cmd) { cmd.undo(g); }, history[currentPos]);
            currentPos--;
        }
    }

    void redo(Graph& g) {
        if (currentPos < (int)history.size() - 1) {
            currentPos++;
            std::visit([&g](auto& cmd) { cmd.execute(g); }, history[currentPos]);
        }
    }

    void clear() {
        history.clear();
        currentPos = -1;
    }

    bool canUndo() const { return currentPos >= 0; }
    bool canRedo() const { return currentPos < (int)history.size() - 1; }
    size_t size() const { return history.size(); }

private:
    std::vector<cmd::Command> history;
    int currentPos = -1;
    static constexpr size_t maxHistorySize = 100;
};
