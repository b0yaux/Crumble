# Coding Conventions

**Analysis Date:** 2026-02-21

## Naming Patterns

**Files:**
- PascalCase: `Node.h`, `Graph.cpp`, `ScriptBridge.h`. Implementation and header files share the same name.

**Functions:**
- camelCase: `update()`, `getVideoOutput()`, `addNode()`, `serialize()`.

**Variables:**
- camelCase: `nodeId`, `executionDirty`, `fboWidth`, `newLayerIndex`.

**Types:**
- PascalCase: `Node`, `Graph`, `Connection`, `Session`, `VideoMixer`.

## Code Style

**Formatting:**
- **Braces:** Same-line braces for class definitions, functions, and control flow blocks.
- **Indentation:** 4 spaces.
- **Separation:** Public/Protected/Private sections are clearly labeled in headers. Virtual methods are often grouped.

**Linting:**
- Not explicitly detected, but the codebase follows a consistent manual style.

## Import Organization

**Order:**
1. Component header (in `.cpp` files): `#include "Node.h"`
2. openFrameworks headers: `#include "ofMain.h"`
3. Standard library headers: `#include <vector>`, `#include <memory>`
4. Other project headers: `#include "../../core/Graph.h"`

**Path Aliases:**
- None detected. Relative paths are used (e.g., `../../core/Graph.h`).

## Error Handling

**Patterns:**
- **Guard Clauses:** Early returns for invalid states or inputs (`if (!json.is_object()) return;`).
- **Defensive Parsing:** A `getSafeJson` helper template is used in `Node.h` to handle type mismatches and missing keys in JSON data.
- **Logging:** Extensive use of `ofLog` (Notice, Verbose, Warning) for runtime tracking and error reporting.

## Logging

**Framework:** openFrameworks built-in `ofLog`.

**Patterns:**
- `ofLogNotice("Category") << "Message";` for significant events.
- `ofLogVerbose("Category") << "Message";` for detailed debugging info.
- `ofLogWarning("Category") << "Message";` for non-critical failures.

## Comments

**When to Comment:**
- Above class definitions for architectural overview.
- Above complex methods to explain logic (e.g., `pullFromNode` in `Graph.h`).
- Inline for specific implementation details or "magic" numbers.

**JSDoc/TSDoc:**
- Uses JSDoc-style blocks for major classes and concepts in headers (e.g., `ScriptBridge.h`).

## Function Design

**Size:** Functions are generally concise and focused on a single responsibility.

**Parameters:** Passed by value for primitives, by `const reference` for strings/JSON, and by `pointer` or `reference` for mutable objects.

**Return Values:** Uses `nullptr` to indicate failure to find or create an object. Uses `bool` for success/failure of operations.

## Module Design

**Exports:** Standard C++ header/implementation pattern.

**Barrel Files:** Not used. Direct includes of headers are required.

---

*Convention analysis: 2026-02-21*
