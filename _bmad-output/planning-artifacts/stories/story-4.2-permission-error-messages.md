---
epic: 4
story: 4.2
title: "Improve Permission Error Messages"
priority: P1
status: draft
key_files:
  - src/scan_progs/send_packet.c
frs_covered:
  - FR14
---

# Story 4.2: Improve Permission Error Messages

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a user who gets "dnet ip_open fails",
I want a clear message telling me what to do,
So that I can fix the problem in one step.

## Implementation Guidance

- Read `src/scan_progs/send_packet.c:1258-1282` — both `SOCK_IP` and `SOCK_LL` error paths
- Use `ERR()` macro per Jack Louis style guide Section 3
- macOS-specific messages must be `#ifdef __APPLE__` guarded
- Chesterton's fence: the current `terminate()` is intentionally terse — Jack's style was minimal. We add platform-specific context without changing the fatal behavior.

**Mantra:** The error message must be actionable on first read. A user should be able to copy-paste the fix command.

## Acceptance Criteria

**Given** `ip_open()` returns NULL on macOS
**When** the error is logged
**Then** `ERR()` prints "raw socket access requires root on macOS" and suggests `sudo`
**And** `terminate()` is still called (fatal)

**Given** either failure occurs on Linux
**When** the error is logged
**Then** existing error messages unchanged

## Key Files

- `src/scan_progs/send_packet.c:1258-1282` — both `SOCK_IP` and `SOCK_LL` error paths where macOS-specific `ERR()` messages must be added under `#ifdef __APPLE__`

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
