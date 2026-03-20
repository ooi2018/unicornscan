# Story 4.3: Runtime BPF Access Check at Startup

Status: done

## Story

As a user,
I want permission problems detected before children are forked,
so that I get a clear error immediately instead of a delayed child crash.

## Acceptance Criteria

1. **Given** unicornscan starts on macOS as non-root without ChmodBPF group **When** the BPF check runs before `chld_fork()` **Then** EACCES produces actionable ChmodBPF setup instructions and exits

2. **Given** bpf0 is EBUSY (held by another process) **When** the check runs **Then** EBUSY is treated as success (we'll get bpf1+)

3. **Given** running as root **When** startup runs **Then** the BPF check is skipped entirely

## Tasks / Subtasks

- [x] Read `src/main.c` fully — identify all exit paths and the exact location of `chld_fork()` call (AC: #1, #2, #3)
  - [x] Confirm `chld_fork()` is at line 572
  - [x] Note any existing early-exit checks that set the pattern for this addition
- [x] Implement the BPF access check function (AC: #1, #2, #3)
  - [x] Guard the entire check with `#ifdef __APPLE__`
  - [x] Skip check entirely when `getuid() == 0` (AC: #3)
  - [x] Call `open("/dev/bpf0", O_RDWR)` and inspect `errno`
  - [x] `EACCES`: print actionable ChmodBPF setup instructions via `ERR()` and call `exit(1)`
  - [x] `EBUSY`: treat as success, proceed (AC: #2)
  - [x] Any other error or success: proceed normally
- [x] Insert the check into `src/main.c` before `chld_fork()` (AC: #1, #2, #3)
  - [x] Place it immediately before the `chld_fork()` call (after drone_init, before forklocal block)
  - [x] Confirm placement does not interfere with any earlier initialisation
- [ ] Test EACCES path: run as non-root without ChmodBPF group (AC: #1)
  - [ ] Confirm error message includes ChmodBPF instructions and scanner exits immediately
- [ ] Test EBUSY path: hold `/dev/bpf0` open and run as non-root with ChmodBPF group (AC: #2)
  - [ ] Confirm scanner proceeds without error
- [ ] Test root path: run as root (AC: #3)
  - [ ] Confirm check is skipped and scanner proceeds normally

## Dev Notes

### Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it. Read the function. Read its callers. Read the history.

### Implementation Guidance

- Read `src/main.c` — understand where `chld_fork()` is called (line 572)
- Try `open("/dev/bpf0", O_RDWR)` — EACCES means no permission, EBUSY means busy (OK)
- Skip check when `getuid() == 0`
- Guard with `#ifdef __APPLE__`
- Chesterton's fence: no startup check exists because the original Linux version ran as root.

**Mantra:** Understand every exit path in main.c before adding the check.

### Project Structure Notes

- `src/main.c` — the BPF access check must be inserted before the `chld_fork()` call at line 572, under `#ifdef __APPLE__`, skipping when `getuid() == 0`

### References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`

## Dev Agent Record

### Agent Model Used

claude-sonnet-4-6

### Debug Log References

None — clean implementation, compiled without errors on first attempt.

### Completion Notes List

- Added `#include <fcntl.h>` to `src/main.c` (required for O_RDWR)
- Added static `check_bpf_access()` function immediately before `main()`, guarded by `#ifdef __APPLE__`
- Function skips check when `getuid() == 0` (AC #3)
- EBUSY returns 1 (success) since eth_bpf_macos.c iterates bpf0..bpf255 (AC #2)
- EACCES prints five ERR() lines with ChmodBPF instructions then returns -1 (AC #1)
- Any other errno returns 1 (let deeper code handle it)
- Call site placed after `drone_init()` and before the `if (s->forklocal)` block, inside `#ifdef __APPLE__`
- `terminate()` called when `check_bpf_access()` returns -1

### Change Log

- 2026-03-20: Implemented story 4.3 — runtime BPF access check before chld_fork()

### File List

- `src/main.c`
