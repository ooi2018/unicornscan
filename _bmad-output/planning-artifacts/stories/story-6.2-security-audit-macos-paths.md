---
epic: 6
story: 6.2
title: "Security Audit of macOS Code Paths"
priority: P1
status: draft
key_files:
  - src/unilib/arch.c
  - src/unilib/xpoll.c
  - src/unilib/intf.c
  - src/unilib/route.c
  - src/usignals.c
frs_covered: []
---

# Story 6.2: Security Audit of macOS Code Paths

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a security engineer,
I want all macOS-specific code reviewed for vulnerabilities.

## Implementation Guidance

- Audit all code behind `#ifdef __APPLE__`, `HAVE_SANDBOX_H`, `AF_LINK`, `HAVE_KQUEUE`
- CFA Agent #7 found 10 findings, 3 medium: `children_dead` not sig_atomic_t, `setgroups()` missing, kqueue fd CLOEXEC non-fatal
- Follow Jack Louis style for all fixes
- Chesterton's fence for each finding: understand why the code was written that way

**Mantra:** Security audit means reading every line, not skimming. Every `#ifdef __APPLE__` block. No assumptions.

## Acceptance Criteria

**Given** all macOS `#ifdef` paths reviewed
**When** audit complete
**Then** `children_dead` is `volatile sig_atomic_t`
**And** `setgroups(1, &mygid)` called before UID/GID drop in `drop_privs()`
**And** no unvalidated buffer operations
**And** signal handlers only use async-signal-safe functions
**And** each finding documented: severity, location, fix, rationale

## Key Files

- `src/unilib/arch.c` — contains `apply_sandbox()` and `drop_privs()`; `setgroups()` call must be added before UID/GID drop
- `src/unilib/xpoll.c` — kqueue fd CLOEXEC handling to be audited
- `src/unilib/intf.c` — `AF_LINK` code paths to be audited for buffer safety
- `src/unilib/route.c` — macOS routing code paths to be audited
- `src/usignals.c` — `children_dead` must be changed to `volatile sig_atomic_t`; all signal handlers must use only async-signal-safe functions

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
