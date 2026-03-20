---
epic: 6
story: 6.3
title: "Validate pcap_setdirection Behavior"
priority: P2
status: draft
key_files:
  - src/scan_progs/recv_packet.c
frs_covered: []
---

# Story 6.3: Validate pcap_setdirection Behavior

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a developer,
I want to confirm scan accuracy without direction filtering.

## Implementation Guidance

- Read `src/scan_progs/recv_packet.c:284-289` — the `pcap_setdirection(PCAP_D_IN)` non-fatal fallback
- Read BPF filter strings (TCP_EFILTER, UDP_EFILTER) — may already exclude own outbound
- Chesterton's fence: direction filtering was added as defense against counting outbound packets as results

## Acceptance Criteria

**Given** `pcap_setdirection(PCAP_D_IN)` succeeds
**When** TCP SYN scan runs against known target
**Then** results match expected

**Given** `pcap_setdirection` fails (non-fatal)
**When** same scan runs
**Then** results identical — no duplicates from outbound reflection

## Key Files

- `src/scan_progs/recv_packet.c:284-289` — the `pcap_setdirection(PCAP_D_IN)` call and its non-fatal fallback; both the success and failure paths must be tested to confirm scan result accuracy

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
