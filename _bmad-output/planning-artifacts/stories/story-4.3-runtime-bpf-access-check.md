---
epic: 4
story: 4.3
title: "Runtime BPF Access Check at Startup"
priority: P2
status: draft
key_files:
  - src/main.c
frs_covered:
  - FR15
---

# Story 4.3: Runtime BPF Access Check at Startup

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a user,
I want permission problems detected before children are forked,
So that I get a clear error immediately instead of a delayed child crash.

## Implementation Guidance

- Read `src/main.c` — understand where `chld_fork()` is called (line 572)
- Try `open("/dev/bpf0", O_RDWR)` — EACCES means no permission, EBUSY means busy (OK)
- Skip check when `getuid() == 0`
- Guard with `#ifdef __APPLE__`
- Chesterton's fence: no startup check exists because the original Linux version ran as root.

**Mantra:** Understand every exit path in main.c before adding the check.

## Acceptance Criteria

**Given** unicornscan starts on macOS as non-root without ChmodBPF group
**When** the BPF check runs before `chld_fork()`
**Then** EACCES produces actionable ChmodBPF setup instructions and exits

**Given** bpf0 is EBUSY (held by another process)
**When** the check runs
**Then** EBUSY is treated as success (we'll get bpf1+)

**Given** running as root
**When** startup runs
**Then** the BPF check is skipped entirely

## Key Files

- `src/main.c` — the BPF access check must be inserted before the `chld_fork()` call at line 572, under `#ifdef __APPLE__`, skipping when `getuid() == 0`

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
