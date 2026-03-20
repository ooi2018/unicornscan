---
epic: 2
story: 2.4
title: "Register atexit() Handler for Child Cleanup"
priority: P2
status: draft
key_files:
  - src/main.c
  - src/chld.c
frs_covered: []
---

# Story 2.4: Register atexit() Handler for Child Cleanup

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a developer,
I want child cleanup even when main() exits normally without going through scan lifecycle,
So that edge-case exits don't leak processes.

## Implementation Guidance

- Read `src/main.c` — understand all exit paths
- Read `src/unilib/terminate.c` — `exit()` runs atexit; `_exit()` does not
- Chesterton's fence: no atexit handler exists because the original design relied on explicit `chld_reapall()` after `run_scan()`. But if `run_scan()` never returns, children leak.

**Mantra:** Understand every exit path in main.c before adding the handler.

## Acceptance Criteria

**Given** `atexit(chld_cleanup)` registered before `chld_fork()`
**When** `main()` exits via `exit()` (including `uexit()` for non-forked main)
**Then** `chld_killall()` runs
**And** the handler is idempotent

## Key Files

- `src/main.c` — all exit paths must be understood; `atexit()` registration goes here before `chld_fork()` is called
- `src/chld.c` — `chld_cleanup` wrapper function (idempotent, calls `chld_killall()`) goes here

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
