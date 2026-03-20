---
epic: 3
story: 3.3
title: "Validate SPI Across macOS Versions"
priority: P1
status: draft
key_files: []
frs_covered: []
---

# Story 3.3: Validate SPI Across macOS Versions

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a developer,
I want confirmation the libsandbox SPI works on macOS 13-26,
So that we don't ship sandbox support that breaks on older systems.

## Implementation Guidance

- This is validation/research, not code
- Test on CI runners or VMs for each macOS version
- The SPI symbols are in SDK TBD stubs from macOS 10.6 through 26.2 — but runtime behavior must be verified

**Mantra:** Do not assume SDK presence equals runtime availability. Test on real systems.

## Acceptance Criteria

**Given** the SPI-based `apply_sandbox()` implementation
**When** tested on macOS 13, 14, 15, and 26
**Then** `sandbox_compile_file` + `sandbox_apply` succeeds on all versions
**And** fallback works when symbols are artificially removed

## Key Files

No source files are changed by this story. Results are recorded as validation evidence. If gaps are found, implementation stories are updated accordingly.

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
