---
epic: 5
story: 5.1
title: "Complete Homebrew Formula"
priority: P0
status: draft
key_files:
  - macos/unicornscan.rb
frs_covered:
  - FR11
  - FR12
---

# Story 5.1: Complete Homebrew Formula

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a macOS user,
I want `brew install unicornscan` to just work,
So that I get a properly configured scanner with all dependencies.

## Implementation Guidance

- Read `macos/unicornscan.rb` (243 lines) fully
- CFA Agent #6 found: sha256 PLACEHOLDER, missing flex/bison deps, no PKG_CONFIG_PATH for keg-only libpcap
- Chesterton's fence: the formula uses DESTDIR staging because Homebrew's standard install relies on `--prefix` being the Cellar path. This is valid Homebrew practice.

**Mantra:** Test on a CLEAN system. Not your dev machine. A fresh macOS VM or `brew install --build-from-source` after `brew uninstall` of any prior version.

## Acceptance Criteria

**Given** the formula with correct sha256
**When** `brew install robertelee78/unicornscan/unicornscan` runs
**Then** all dependencies installed, build completes, all binaries present
**And** `brew test unicornscan` passes
**And** `brew audit --strict unicornscan` produces zero errors
**And** `brew uninstall unicornscan` cleanly removes all files

## Key Files

- `macos/unicornscan.rb` — the 243-line Homebrew formula; sha256 must be corrected, flex/bison deps added, PKG_CONFIG_PATH set for keg-only libpcap

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
