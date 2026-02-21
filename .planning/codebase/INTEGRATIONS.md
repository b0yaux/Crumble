# External Integrations

**Analysis Date:** 2026-02-21

## APIs & External Services

**Not Detected:**
- No external web APIs (REST, GraphQL, etc.) were found in the codebase.
- No cloud service SDKs (AWS, Google Cloud, Firebase) are currently integrated.

## Data Storage

**Databases:**
- Not detected.

**File Storage:**
- **Local Filesystem:** Primary storage for media assets (video and audio) and session state.
  - Implementation: Uses `std::filesystem` (C++23) and openFrameworks `ofFile` utilities.
  - Formats: `.json` for session state, `.mov`/`.mp4`/`.hap` for video, `.wav` for audio.

**Caching:**
- **None:** The application loads assets directly from the filesystem into memory/GPU as needed via `AssetPool.h`.

## Authentication & Identity

**Auth Provider:**
- **None:** No authentication or identity management system detected. The application is a standalone desktop tool.

## Monitoring & Observability

**Error Tracking:**
- **None:** No external error tracking services (Sentry, LogRocket, etc.) are used.

**Logs:**
- **Standard Console Logs:** Uses openFrameworks `ofLog` system.
  - Patterns: `ofLogNotice`, `ofLogWarning`, `ofLogError`.

## CI/CD & Deployment

**Hosting:**
- **Desktop Application:** Deployment target is macOS.

**CI Pipeline:**
- **GitHub Actions (Potential):** Not explicitly detected in `.github/` during root scan, but typical for openFrameworks projects.

## Environment Configuration

**Required env vars:**
- **None:** The application does not use environment variables for configuration.

**Secrets location:**
- **Not applicable:** No secrets or credentials were found in the codebase.

## Webhooks & Callbacks

**Incoming:**
- **None:** No webhook listeners or incoming network callbacks detected.

**Outgoing:**
- **None:** No outgoing webhooks or notification services integrated.

---

*Integration audit: 2026-02-21*
