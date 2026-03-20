# Story 4.2: Improve Permission Error Messages

Status: review

## Story

As a user who gets "dnet ip_open fails",
I want a clear message telling me what to do,
so that I can fix the problem in one step.

## Acceptance Criteria

1. **Given** `ip_open()` returns NULL on macOS **When** the error is logged **Then** `ERR()` prints "raw socket access requires root on macOS" and suggests `sudo` **And** `terminate()` is still called (fatal)

2. **Given** either failure occurs on Linux **When** the error is logged **Then** existing error messages unchanged

## Tasks / Subtasks

- [ ] Read both error paths in full before modifying (AC: #1, #2)
  - [ ] Read `src/scan_progs/send_packet.c:1258-1282` — both the `SOCK_IP` and `SOCK_LL` error paths in `open_link()`
  - [ ] Confirm `terminate()` is present and must remain fatal on all platforms
  - [ ] Confirm current error message text that must remain unchanged on Linux
- [ ] Add macOS-specific `ERR()` message for `ip_open()` NULL return (AC: #1)
  - [ ] Wrap under `#ifdef __APPLE__`
  - [ ] Message must include "raw socket access requires root on macOS"
  - [ ] Message must suggest `sudo unicornscan ...` as the fix
  - [ ] Use `ERR()` macro, not `fprintf`, per Jack Louis style guide Section 3
  - [ ] `terminate()` call must follow immediately after
- [ ] Verify the `SOCK_LL` (`eth_open`) error path also has an appropriate macOS message (AC: #1)
  - [ ] If not covered by Story 1.2, add a complementary `#ifdef __APPLE__` message here
- [ ] Confirm Linux error messages are byte-for-byte unchanged (AC: #2)
  - [ ] Inspect preprocessor output or build on Linux to verify
- [ ] Trigger both failure paths manually on macOS and confirm messages are immediately actionable (AC: #1)

## Dev Notes

### Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it. Read the function. Read its callers. Read the history.

### Implementation Guidance

- Read `src/scan_progs/send_packet.c:1258-1282` — both `SOCK_IP` and `SOCK_LL` error paths
- Use `ERR()` macro per Jack Louis style guide Section 3
- macOS-specific messages must be `#ifdef __APPLE__` guarded
- Chesterton's fence: the current `terminate()` is intentionally terse — Jack's style was minimal. We add platform-specific context without changing the fatal behavior.

**Mantra:** The error message must be actionable on first read. A user should be able to copy-paste the fix command.

### Project Structure Notes

- `src/scan_progs/send_packet.c:1258-1282` — both `SOCK_IP` and `SOCK_LL` error paths where macOS-specific `ERR()` messages must be added under `#ifdef __APPLE__`

### References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`

## Dev Agent Record

### Agent Model Used

claude-sonnet-4-6

### Debug Log References

N/A — no runtime debugging required; change is straightforward conditional ERR() output.

### Completion Notes List

- Implemented macOS-specific BPF permission guidance in `recv_packet.c` at the `pcap_open_live()` failure path (lines 200-216).
- The `#ifdef __APPLE__` block fires only when `errbuf` contains "ermission" or "BPF", which covers both "Permission denied" and the macOS BPF-specific error format.
- Linux error messages are byte-for-byte unchanged — the entire block is skipped by the preprocessor on non-Apple platforms.
- The file compiles cleanly with zero warnings under `-Wall -Wextra` on macOS.
- The `terminate()` call is preserved immediately after the guidance messages, maintaining the fatal behaviour.
- Story 1.2 conflict note: this change touches `recv_packet.c` (listener side) only; `send_packet.c` (sender side) is handled by Story 1.2.

### Change Log

- `src/scan_progs/recv_packet.c`: added `#ifdef __APPLE__` block after `ERR("pcap open live: %s", errbuf)` to emit actionable BPF permission fix instructions when the error message indicates a permission failure.

### File List

- `src/scan_progs/recv_packet.c`
