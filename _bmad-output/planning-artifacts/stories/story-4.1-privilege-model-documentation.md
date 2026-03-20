---
epic: 4
story: 4.1
title: "Create Privilege Model Reference Documentation"
priority: P1
status: draft
key_files:
  - docs/PRIVILEGES-macos.md
frs_covered:
  - FR16
---

# Story 4.1: Create Privilege Model Reference Documentation

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a macOS user,
I want to know exactly which scan modes need sudo,
So that I can configure my system correctly once and not guess.

## Implementation Guidance

- Read `src/scan_progs/send_packet.c:1233-1282` — `open_link()` to understand SOCK_IP vs SOCK_LL paths
- Read CFA Agent #5 research — confirmed `ip_open()` → `SOCK_RAW` → root required, `eth_open()` → BPF → ChmodBPF sufficient
- Chesterton's fence: no privilege doc exists because the Linux version assumed root. The macOS port created a new privilege tier that needs documentation.

**Mantra:** Verify every cell in the privilege matrix by actually testing it. Don't copy from the PRD without confirming.

## Acceptance Criteria

**Given** a new `docs/PRIVILEGES-macos.md`
**When** a user reads it
**Then** every scan mode has a clear row: mode flag, sender path, privilege requirement
**And** ChmodBPF setup instructions included with exact commands
**And** troubleshooting section covers "ip_open fails", "eth_open fails", "pcap permission denied"

## Key Files

- `docs/PRIVILEGES-macos.md` (new) — the privilege reference document; every claim must be verified by reading `send_packet.c` and actual testing

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
