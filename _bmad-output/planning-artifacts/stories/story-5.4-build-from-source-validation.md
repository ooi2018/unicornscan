---
epic: 5
story: 5.4
title: "Validate Build-from-Source Path"
priority: P1
status: draft
key_files:
  - docs/INSTALL-source.md
frs_covered: []
---

# Story 5.4: Validate Build-from-Source Path

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a developer contributing to unicornscan,
I want the source build to work on a clean macOS.

## Implementation Guidance

- Test the documented build steps on a clean system with only Xcode CLT + Homebrew

## Acceptance Criteria

**Given** a clean macOS
**When** `autoreconf -fiv && ./configure && make && sudo make install` runs
**Then** build succeeds with zero errors, scanner runs, modules load

## Key Files

- `docs/INSTALL-source.md` — the source build instructions document; must reflect the tested build steps exactly as they were verified on a clean system

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
