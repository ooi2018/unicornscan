---
epic: 5
story: 5.3
title: "DMG Installer Validation"
priority: P1
status: draft
key_files:
  - macos/dmg/build-dmg.sh
  - macos/dmg/postinstall
frs_covered:
  - FR13
---

# Story 5.3: DMG Installer Validation

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a user who doesn't use Homebrew,
I want a working DMG installer.

## Implementation Guidance

- Read `macos/dmg/build-dmg.sh` (594 lines) fully
- CFA Agent #6 found: invalid background.png, missing unicornscan-alicorn, deprecated `launchctl load`
- Chesterton's fence: the DMG script was written during v0.5.0 but never tested end-to-end. Treat as unvalidated.

**Mantra:** Build the DMG. Mount it. Install on a clean system. Run a scan. Uninstall. Verify zero files remain.

## Acceptance Criteria

**Given** `build-dmg.sh` produces a DMG
**When** installed on a clean macOS
**Then** all binaries present and linked correctly (`otool -L` shows no /opt/homebrew paths)
**And** modules load, config files installed, ChmodBPF LaunchDaemon loadable
**And** uninstall removes all files

## Key Files

- `macos/dmg/build-dmg.sh` — the 594-line DMG build script; invalid background.png, missing unicornscan-alicorn, and deprecated `launchctl load` must all be fixed
- `macos/dmg/postinstall` — the post-install script; must be verified end-to-end on a clean system

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
