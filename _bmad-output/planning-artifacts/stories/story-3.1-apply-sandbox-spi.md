---
epic: 3
story: 3.1
title: "Implement apply_sandbox() via libsandbox SPI"
priority: P1
status: draft
key_files:
  - src/unilib/arch.c
  - configure.ac
  - macos/unicornscan-listener.sb
frs_covered:
  - FR1
  - FR2
---

# Story 3.1: Implement apply_sandbox() via libsandbox SPI

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a security engineer,
I want the listener process sandboxed on macOS,
So that a crafted packet exploiting a parser bug cannot access the full filesystem or spawn processes.

## Implementation Guidance

- Read `src/unilib/arch.c` fully — understand `apply_sandbox()` (lines 197-215, current stub), `drop_privs()` (lines 218-355), and how they interact
- Read `macos/unicornscan-listener.sb` — the 188-line deny-default Seatbelt profile ready to use
- Read `configure.ac` — the `HAVE_SANDBOX_INIT` detection and `SANDBOX_PROFILE` define
- The SPI was **tested and confirmed working on macOS 26.3.1** during CFA research
- Use `dlopen("/usr/lib/libsandbox.1.dylib", RTLD_LAZY | RTLD_LOCAL)` + `dlsym()` for each symbol
- The code sketch in PRD Section 3.2.1 is the reference implementation
- Chesterton's fence: `sandbox_init()` was the original intent. It failed because `SANDBOX_NAMED` only accepts Apple's built-in profile constants. The SPI bypasses this by going directly to the underlying implementation.

**Mantra:** This code touches security boundaries. Test sandbox enforcement by verifying a sandboxed listener CANNOT write to `/tmp/test_sandbox_escape`. Test graceful degradation with libsandbox removed. No shortcuts.

## Acceptance Criteria

**Given** unicornscan is running on macOS with the `.sb` profile installed
**When** the listener calls `apply_sandbox()` during `drop_privs()`
**Then** `dlopen` loads `libsandbox.1.dylib`, `dlsym` resolves all SPI symbols
**And** `sandbox_compile_file(SANDBOX_PROFILE, params, &error)` compiles the profile
**And** `sandbox_apply(profile)` enforces the deny-default policy
**And** scan results are identical with and without sandbox

**Given** `libsandbox.1.dylib` is missing or SPI symbols not found
**When** `dlopen` or `dlsym` fails
**Then** VRB(1) logs the reason and returns 0 (non-fatal)
**And** the scan proceeds without sandbox

**Given** unicornscan is built on Linux
**When** compiled
**Then** `#ifdef HAVE_SANDBOX_H` excludes all macOS sandbox code

## Key Files

- `src/unilib/arch.c:197-215` — `apply_sandbox()` current stub to be replaced with the SPI-based implementation
- `configure.ac` — `HAVE_SANDBOX_INIT` detection and `SANDBOX_PROFILE` path definition
- `macos/unicornscan-listener.sb` — the 188-line deny-default Seatbelt profile to be applied

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
