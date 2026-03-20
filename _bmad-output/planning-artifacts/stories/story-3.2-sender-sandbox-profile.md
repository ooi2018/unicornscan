---
epic: 3
story: 3.2
title: "Create Sender Sandbox Profile"
priority: P2
status: draft
key_files:
  - macos/unicornscan-sender.sb
  - src/unilib/arch.c
frs_covered: []
---

# Story 3.2: Create Sender Sandbox Profile

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a security engineer,
I want the sender process (unisend) also sandboxed,
So that both child processes have defense-in-depth isolation.

## Implementation Guidance

- Read `macos/unicornscan-listener.sb` to understand the profile structure
- Read `src/scan_progs/send_packet.c` to understand what the sender needs: BPF write, IPC socket, sysctl read, module loading
- Chesterton's fence: no sender profile exists because the original plan only sandboxed the listener (which processes untrusted network data). Sandboxing the sender is defense-in-depth.

**Mantra:** Write the profile by reading every syscall the sender makes, not by guessing. Use `sandbox-exec -f profile.sb -- /path/to/unisend` to test interactively.

## Acceptance Criteria

**Given** a new `macos/unicornscan-sender.sb` profile
**When** the sender process starts with sandbox applied
**Then** BPF device write is allowed, IPC sockets allowed, sysctl read allowed
**And** file creation denied, process spawning denied, fork denied
**And** TCP SYN, UDP, and ARP scans produce correct results

## Key Files

- `macos/unicornscan-sender.sb` (new) — the Seatbelt profile for the sender process; must be derived by reading every syscall in `send_packet.c`
- `src/unilib/arch.c` — `apply_sandbox()` will need a way to select the correct profile (listener vs sender)

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
