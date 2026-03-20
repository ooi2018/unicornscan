---
epic: 6
story: 6.1
title: "ICMP Scan Mode Status Assessment"
priority: P1
status: draft
key_files:
  - src/scan_progs/scanopts.c
  - src/scan_progs/send_packet.c
  - src/scan_progs/workunits.c
frs_covered:
  - FR10
---

# Story 6.1: ICMP Scan Mode Status Assessment

## Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it.

## User Story

As a developer,
I want to determine whether ICMP scanning works, is partial, or is a dead stub,
So that we can validate it or document it as unsupported.

## Implementation Guidance

- Read `src/scan_progs/scanopts.c:474-563` — does `scan_parsemode()` have an `'I'` branch?
- Read `src/scan_progs/send_packet.c:527-562` — does the workunit switch handle `ICMP_SEND_MAGIC`?
- CFA Agent #7 found: MODE_ICMPSCAN defined (value 8), magic numbers exist, but `scan_parsemode()` has NO `'I'` branch — dead stub
- Chesterton's fence: ICMP mode was likely planned but never finished in the original 0.4.7

**Mantra:** Do NOT implement ICMP mode in this story. This is ASSESSMENT ONLY. Determine exact state, document, recommend.

## Acceptance Criteria

**Given** the MODE_ICMPSCAN code paths are audited
**When** the assessment is complete
**Then** a clear determination: working / partial / dead stub
**And** if dead stub: recommendation to implement or remove scaffolding
**And** GETTING_STARTED.md updated to reflect actual ICMP status

## Key Files

- `src/scan_progs/scanopts.c` — `scan_parsemode()` at lines 474-563; check whether an `'I'` branch exists
- `src/scan_progs/send_packet.c` — workunit switch at lines 527-562; check whether `ICMP_SEND_MAGIC` is handled
- `src/scan_progs/workunits.c` — ICMP workunit definitions to be assessed for completeness

## References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`
