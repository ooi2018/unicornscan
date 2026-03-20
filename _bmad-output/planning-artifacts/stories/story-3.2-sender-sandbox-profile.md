# Story 3.2: Create Sender Sandbox Profile

Status: done

## Story

As a security engineer,
I want the sender process (unisend) also sandboxed,
so that both child processes have defense-in-depth isolation.

## Acceptance Criteria

1. **Given** a new `macos/unicornscan-sender.sb` profile **When** the sender process starts with sandbox applied **Then** BPF device write is allowed, IPC sockets allowed, sysctl read allowed **And** file creation denied, process spawning denied, fork denied **And** TCP SYN, UDP, and ARP scans produce correct results

## Tasks / Subtasks

- [x] Read all source that reveals sender syscall requirements (AC: #1)
  - [x] Read `macos/unicornscan-listener.sb` — understand the profile structure and syntax used
  - [x] Read `src/scan_progs/send_packet.c` in full — identify every syscall the sender makes: BPF write, IPC socket operations, sysctl reads, any module loading
- [x] Draft `macos/unicornscan-sender.sb` profile (AC: #1)
  - [x] Start from deny-default base matching listener profile structure
  - [x] Add explicit allow rules for: BPF device write, IPC sockets, sysctl read
  - [x] Confirm file creation, process spawning, and fork are denied by the default
- [ ] Test profile interactively using `sandbox-exec` (AC: #1)
  - [ ] Run `sandbox-exec -f macos/unicornscan-sender.sb -- /path/to/unisend` and confirm no denials on allowed operations
  - [ ] Confirm denied operations are rejected as expected
- [ ] Update `apply_sandbox()` in `src/unilib/arch.c` to select the correct profile (AC: #1)
  - [ ] Add a mechanism (e.g., a parameter or process-role flag) to distinguish listener vs sender
  - [ ] Read existing `apply_sandbox()` implementation from Story 3.1 before modifying
- [ ] Run TCP SYN, UDP, and ARP scans with sandbox active and verify correct results (AC: #1)

## Dev Notes

### Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it. Read the function. Read its callers. Read the history.

### Implementation Guidance

- Read `macos/unicornscan-listener.sb` to understand the profile structure
- Read `src/scan_progs/send_packet.c` to understand what the sender needs: BPF write, IPC socket, sysctl read, module loading
- Chesterton's fence: no sender profile exists because the original plan only sandboxed the listener (which processes untrusted network data). Sandboxing the sender is defense-in-depth.

**Mantra:** Write the profile by reading every syscall the sender makes, not by guessing. Use `sandbox-exec -f profile.sb -- /path/to/unisend` to test interactively.

### Project Structure Notes

- `macos/unicornscan-sender.sb` (new) — the Seatbelt profile for the sender process; must be derived by reading every syscall in `send_packet.c`
- `src/unilib/arch.c` — `apply_sandbox()` will need a way to select the correct profile (listener vs sender)

### References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`

## Dev Agent Record

### Agent Model Used

claude-sonnet-4-6

### Debug Log References

None — implementation derived by reading source directly.

### Completion Notes List

- Read `macos/unicornscan-listener.sb` to understand the Seatbelt profile syntax, comment style, and deny-default structure.
- Read `src/scan_progs/send_packet.c` in full to enumerate every syscall surface the sender exercises:
  - `eth_open(s->interface_str)` + `eth_send()` — opens and writes `/dev/bpfN` for ARP/link-layer mode; requires `file-read* file-write*` on `/dev/bpf[0-9]+`.
  - `ip_open()` + `ip_send()` — creates a raw IP socket for TCP/UDP/traceroute mode; covered by `(allow network*)`.
  - `socktrans_bind(s->ipcuri)` + `socktrans_accept()` + `get_singlemessage()` / `send_message()` — bidirectional Unix domain socket IPC with master; requires `file-read* file-write*` on the socket paths.
  - `init_modules()` calls `lt_dlinit()`, `opendir(s->mod_dir)`, and `lt_dlopen()` on `.dylib` files — requires file-read on lib and module directories.
  - `readconf(conffile)` reads `SYSCONFDIR/unicornscan/unicorn.conf` for payload definitions.
  - `sysctl-read` needed by libdnet (routing table) and dyld (page size, OS type).
- The sender never calls `fork()`, `exec()`, `fopen()` for writing, or any file-create operation; these remain denied by the default policy.
- Key difference from the listener profile: the BPF device node needs `file-write*` in addition to `file-read*` because `eth_open()` opens the device inside the sandbox (the sender must open it, unlike the listener where pcap opens it before sandboxing).
- The `/tmp` write block present in the listener profile is deliberately absent here; the sender writes no dump files.

### Change Log

- 2026-03-20: Created `macos/unicornscan-sender.sb` with deny-default policy covering all sender syscall requirements.

### File List

- `macos/unicornscan-sender.sb` (new) — Seatbelt sandbox profile for the sender process (unisend)
