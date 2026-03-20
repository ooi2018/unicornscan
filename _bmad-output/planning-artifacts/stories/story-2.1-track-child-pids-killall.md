---
epic: 2
story: 2.1
title: "Track Child PIDs and Implement chld_killall()"
priority: P1
status: draft
key_files:
  - src/chld.c
  - src/chld.h
frs_covered:
  - FR6
  - FR7
---

# Story 2.1: Track Child PIDs and Implement chld_killall()

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a user,
I want unicornscan to always clean up its child processes,
So that stale unisend/unilisten never persist after a scan ends or crashes.

## Implementation Guidance

- Read `src/chld.c` fully — understand the fork/exec lifecycle, `child_forked` counter, and how `chld_reapall()` works today
- Read `src/chld.h` for the public API surface
- Read `src/main.c` to see where `chld_reapall()` is called (line 748, after `run_scan()`)
- Read `src/usignals.c:81-91` to understand the SIGCHLD handler interaction
- Chesterton's fence: `child_forked` is an int, not a PID array, because Jack's original design relied on IPC-based lifecycle (not PID tracking). The IPC approach breaks when children die without sending MSG_WORKDONE — that's the bug we're fixing.
- Use `MAX_CHILDREN` (already defined as 16 in `chld.c:36`) for array size
- Follow Jack Louis style: static module-level array, `/* */` comments, explicit `return;` on void functions

**Mantra:** Read `chld.c`, `usignals.c`, `main.c`, and `master.c` before writing a single line. Understand ALL the ways children are created, tracked, and reaped. Measure 3x, cut once.

## Acceptance Criteria

**Given** `chld_fork()` successfully forks a child (sender or listener)
**When** the child PID is returned by `fork()`
**Then** it is stored in a static `child_pids[MAX_CHILDREN]` array at index `child_forked`
**And** the existing `child_forked++` counter still increments

**Given** the master process exits (normal, error, or SIGTERM)
**When** cleanup runs
**Then** `chld_killall()` sends `SIGTERM` to every tracked PID where `child_pids[j] > 0`
**And** waits 500ms via `usleep(500000)`
**And** sends `SIGKILL` to any child still alive (checked via `kill(pid, 0) == 0`)
**And** `kill()` errors with `ESRCH` are silently ignored (child already dead)

## Key Files

- `src/chld.c` — contains the fork/exec lifecycle, `child_forked` counter, and `chld_reapall()`; the `child_pids[]` array and `chld_killall()` go here
- `src/chld.h` — public API surface; `chld_killall()` declaration goes here

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
