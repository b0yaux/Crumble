# Testing Patterns

**Analysis Date:** 2026-02-21

## Test Framework

**Runner:**
- Not detected. Formal automated testing frameworks (like Catch2 or Google Test) are not present in the current codebase.

**Assertion Library:**
- Not detected.

**Run Commands:**
- Not applicable. Testing currently appears to be manual/visual within the openFrameworks application context.

## Test File Organization

**Location:**
- No dedicated test directory (e.g., `tests/` or `spec/`) was found.

**Naming:**
- Not applicable.

## Test Structure

**Suite Organization:**
- Not applicable.

**Patterns:**
- Manual verification via UI/Logs: The presence of `ofLogVerbose` and `ofLogNotice` suggests that developers rely on console output to verify internal state.
- Visual Verification: As a creative coding tool, output is verified visually via `ScreenOutput` and `GraphUI`.

## Mocking

**Framework:** None detected.

**Patterns:**
- Not applicable.

## Fixtures and Factories

**Test Data:**
- `ofApp::addTestLayers(int count)` in `src/ofApp.cpp` serves as a manual "smoke test" or fixture to populate the graph with dummy data for UI testing.

**Location:**
- Within `ofApp.cpp` for integration/manual testing.

## Coverage

**Requirements:** None enforced.

## Test Types

**Unit Tests:**
- Not detected.

**Integration Tests:**
- Manual integration testing via `ScriptBridge` (running Lua scripts to build graphs).

**E2E Tests:**
- Not used.

## Common Patterns

**Async Testing:**
- Not applicable.

**Error Testing:**
- Manual verification of error logs when providing invalid JSON or Lua scripts.

---

*Testing analysis: 2026-02-21*
