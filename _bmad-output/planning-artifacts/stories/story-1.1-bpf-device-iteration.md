---
epic: 1
story: 1.1
title: "Patch Bundled libdnet BPF Device Iteration"
priority: P1
status: draft
key_files:
  - libs/
  - libs/Makefile.in
frs_covered:
  - FR3
  - FR4
  - FR5
---

# Story 1.1: Patch Bundled libdnet BPF Device Iteration

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a penetration tester running multiple tools,
I want `eth_open()` to find any available BPF device,
So that I never get "Resource busy" errors when `/dev/bpf0` is held by another process.

## Implementation Guidance

- Read `libs/` bundled libdnet source fully — understand how `eth-bsd.c` works before touching it
- Study how libpcap iterates BPF devices in `pcap-bpf.c` for reference
- The patch must be conditional on `__APPLE__` or BSD — Linux uses `PF_PACKET`, not BPF
- Follow Jack Louis style: `/* */` comments, `return -1` on error, no `//` comments
- Reference: `docs/jack-louis-coding-style-guide.md`
- Chesterton's fence: understand why libdnet hardcoded `/dev/bpf0` (likely a simplification from 2004 when BSD systems had fewer BPF consumers)

**Mantra:** DO NOT BE LAZY. Read the entire `eth-bsd.c` before patching. Verify the patch compiles on macOS AND that Linux builds skip it entirely. Test with bpf0 held by another process. Measure 3x, cut once.

## Acceptance Criteria

**Given** `/dev/bpf0` is held by another process (e.g., Wireshark, pcap, unilisten)
**When** unicornscan's sender calls `eth_open()` for ARP mode (`send_packet.c:1270`)
**Then** libdnet iterates `/dev/bpf1` through `/dev/bpf255` until a free device is found
**And** the BPF fd is bound to the correct interface via `BIOCSETIF`
**And** `BIOCSHDRCMPLT` is set for header-complete mode

**Given** all 256 BPF devices are busy
**When** `eth_open()` exhausts the iteration
**Then** it returns NULL (same as current behavior)
**And** the caller's error handling is triggered

**Given** unicornscan is built on Linux
**When** the bundled libdnet is compiled
**Then** the BPF iteration patch is not applied (guarded by `uname -s` check in `libs/Makefile.in`)
**And** Linux behavior is completely unchanged

## Key Files

- `libs/` — bundled libdnet source, specifically `eth-bsd.c` which contains the `eth_open()` implementation to be patched
- `libs/Makefile.in` — controls conditional compilation; the Linux guard must live here

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
