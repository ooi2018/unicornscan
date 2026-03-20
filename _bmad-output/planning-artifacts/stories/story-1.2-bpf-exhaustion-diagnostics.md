---
epic: 1
story: 1.2
title: "BPF Device Exhaustion Diagnostics"
priority: P1
status: draft
key_files:
  - src/scan_progs/send_packet.c
frs_covered: []
---

# Story 1.2: BPF Device Exhaustion Diagnostics

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a user who encounters "eth_open fails",
I want a clear error message explaining what happened and how to fix it,
So that I can resolve the issue without searching online.

## Implementation Guidance

- Read `send_packet.c:1258-1282` (`open_link()`) fully before modifying
- Read how `fantaip.c` handles the same error for consistency
- Use `ERR()` macro (not fprintf), per Jack Louis style guide Section 3
- The diagnostic must be macOS-only (`#ifdef __APPLE__`) — Linux messages unchanged
- Chesterton's fence: the current `terminate()` call is intentional — a link-layer failure is fatal for ARP mode. Keep it fatal, just make the message better.

**Mantra:** No stubs. The error message must be immediately actionable on first read.

## Acceptance Criteria

**Given** `eth_open()` returns NULL on macOS
**When** the error is logged in `send_packet.c`
**Then** the message includes "all /dev/bpf* devices may be busy"
**And** suggests `lsof /dev/bpf*` to identify holders
**And** the process still calls `terminate()` (fatal for link-layer mode)

**Given** `eth_open()` returns NULL on Linux
**When** the error is logged
**Then** the existing error message is unchanged
**And** no macOS-specific text appears

## Key Files

- `src/scan_progs/send_packet.c:1268-1274` — `open_link()` error handling path where the improved message must be inserted

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
