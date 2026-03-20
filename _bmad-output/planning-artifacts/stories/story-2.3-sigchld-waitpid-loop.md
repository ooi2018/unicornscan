---
epic: 2
story: 2.3
title: "Harden SIGCHLD Handler with waitpid Loop"
priority: P2
status: draft
key_files:
  - src/usignals.c
frs_covered:
  - FR9
---

# Story 2.3: Harden SIGCHLD Handler with waitpid Loop

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a developer,
I want all children reaped atomically even when multiple die simultaneously,
So that zombie processes are impossible.

## Implementation Guidance

- Read `src/usignals.c:81-91` fully — the `signals_chlddead()` handler
- Read `src/usignals.c:29` — `children_dead` is `static int`, not `volatile sig_atomic_t`
- Chesterton's fence: `wait(&status)` was used because the original code only ever had 2 children. On macOS, signal coalescing is more aggressive.
- `wait()` and `waitpid()` are both async-signal-safe per POSIX. `DBG()` is NOT — do NOT add debug output inside the signal handler.

**Mantra:** Signal handlers are the most dangerous code in any C program. Only async-signal-safe functions allowed.

## Acceptance Criteria

**Given** the SIGCHLD handler
**When** two children die simultaneously
**Then** `waitpid(-1, &status, WNOHANG)` loop reaps both
**And** `children_dead` is `volatile sig_atomic_t`
**And** no non-async-signal-safe functions in the handler

## Key Files

- `src/usignals.c:29` — `children_dead` declaration that must be changed to `volatile sig_atomic_t`
- `src/usignals.c:81-91` — the `signals_chlddead()` SIGCHLD handler to be hardened with a `waitpid` loop

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
