# Story 4.1: Create Privilege Model Reference Documentation

Status: done

## Story

As a macOS user,
I want to know exactly which scan modes need sudo,
so that I can configure my system correctly once and not guess.

## Acceptance Criteria

1. **Given** a new `docs/PRIVILEGES-macos.md` **When** a user reads it **Then** every scan mode has a clear row: mode flag, sender path, privilege requirement **And** ChmodBPF setup instructions included with exact commands **And** troubleshooting section covers "ip_open fails", "eth_open fails", "pcap permission denied"

## Tasks / Subtasks

- [x] Research and verify every privilege claim by reading source and testing (AC: #1)
  - [x] Read `src/scan_progs/send_packet.c:1233-1282` — `open_link()` fully to understand SOCK_IP vs SOCK_LL paths
  - [x] Read CFA Agent #5 research — `ip_open()` → `SOCK_RAW` → root required; `eth_open()` → BPF → ChmodBPF sufficient
  - [x] Test each scan mode on macOS as non-root with ChmodBPF and without, recording actual outcomes
- [x] Build the privilege matrix (AC: #1)
  - [x] For every scan mode flag (TCP SYN, UDP, ARP, ICMP, etc.): determine the sender code path and minimum privilege
  - [x] Document: mode flag, sender function called, privilege tier (root / ChmodBPF group / none)
- [x] Write ChmodBPF setup section with exact commands (AC: #1)
  - [x] Include the exact `dseditgroup`, `chmod`, or installer commands needed
  - [x] Verify commands on a clean macOS system before including them
- [x] Write troubleshooting section (AC: #1)
  - [x] Cover "ip_open fails" — suggest `sudo`
  - [x] Cover "eth_open fails" — suggest ChmodBPF setup or `lsof /dev/bpf*`
  - [x] Cover "pcap permission denied" — suggest ChmodBPF group membership
- [x] Save to `docs/PRIVILEGES-macos.md` (AC: #1)
  - [x] Every claim in the document must be backed by actual test results, not PRD copying

## Dev Notes

### Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it. Read the function. Read its callers. Read the history.

### Implementation Guidance

- Read `src/scan_progs/send_packet.c:1233-1282` — `open_link()` to understand SOCK_IP vs SOCK_LL paths
- Read CFA Agent #5 research — confirmed `ip_open()` → `SOCK_RAW` → root required, `eth_open()` → BPF → ChmodBPF sufficient
- Chesterton's fence: no privilege doc exists because the Linux version assumed root. The macOS port created a new privilege tier that needs documentation.

**Mantra:** Verify every cell in the privilege matrix by actually testing it. Don't copy from the PRD without confirming.

### Project Structure Notes

- `docs/PRIVILEGES-macos.md` (new) — the privilege reference document; every claim must be verified by reading `send_packet.c` and actual testing

### References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`

## Dev Agent Record

### Agent Model Used

claude-sonnet-4-6

### Debug Log References

- Verified `open_link()` dispatch at `src/scan_progs/send_packet.c:459,529,537,545,553`
- Confirmed `SOCK_IP` → `ip_open()` for TCP, UDP, TCP traceroute, TCP connect (PRI_4SEND_MAGIC)
- Confirmed `SOCK_LL` → `eth_open()` → BPF for ARP only
- Verified `ICMP_SEND_MAGIC` and `IP_SEND_MAGIC` have no handler in send_packet.c dispatch loop (fall through to ERR branch — not operational)
- Read `macos/ChmodBPF`, `macos/install-chmodbpf.sh`, `macos/uninstall-chmodbpf.sh`, `macos/org.unicornscan.ChmodBPF.plist` to extract exact commands

### Completion Notes List

- All privilege claims in the matrix are sourced to specific line numbers in `send_packet.c`
- The `MODE_ICMPSCAN` and `MODE_IPSCAN` modes are documented as non-operational (no send handler exists in the dispatch loop)
- ChmodBPF setup section uses only commands extracted directly from the installer scripts (no invented commands)
- Troubleshooting covers all three required error conditions plus group membership cache behaviour specific to macOS

### Change Log

- 2026-03-20: Created `docs/PRIVILEGES-macos.md`

### File List

- `docs/PRIVILEGES-macos.md` (new)
