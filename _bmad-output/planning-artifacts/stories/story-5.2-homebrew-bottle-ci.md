---
epic: 5
story: 5.2
title: "Homebrew Bottle Generation in CI"
priority: P1
status: draft
key_files:
  - .github/workflows/release.yml
  - macos/unicornscan.rb
frs_covered: []
---

# Story 5.2: Homebrew Bottle Generation in CI

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a user,
I want pre-compiled bottles so install takes seconds not minutes.

## Implementation Guidance

- Read `.github/workflows/release.yml` — the `build-macos` job
- Bottles are platform-specific: `arm64_sonoma` for Apple Silicon

## Acceptance Criteria

**Given** a new version tag is pushed
**When** the `build-macos` CI job runs
**Then** it produces a `.bottle.tar.gz` artifact uploaded to GitHub Release
**And** formula includes `bottle do` block with correct sha256

## Key Files

- `.github/workflows/release.yml` — the `build-macos` CI job that must be updated to produce and upload the `.bottle.tar.gz` artifact
- `macos/unicornscan.rb` — must include a `bottle do` block with correct sha256 after bottle generation

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
