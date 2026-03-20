# Story 6.2: Security Audit of macOS Code Paths

Status: done

## Story

As a security engineer,
I want all macOS-specific code reviewed for vulnerabilities,
so that the macOS port does not introduce security regressions compared to the Linux baseline.

## Acceptance Criteria

1. **Given** all macOS `#ifdef` paths reviewed **When** audit complete **Then** `children_dead` is `volatile sig_atomic_t` **And** `setgroups(1, &mygid)` called before UID/GID drop in `drop_privs()` **And** no unvalidated buffer operations **And** signal handlers only use async-signal-safe functions **And** each finding documented: severity, location, fix, rationale

## Tasks / Subtasks

- [x] Enumerate every macOS-specific code block before auditing (AC: #1)
  - [x] Search for all `#ifdef __APPLE__`, `#ifdef HAVE_SANDBOX_H`, `#ifdef AF_LINK`, and `#ifdef HAVE_KQUEUE` blocks across the entire codebase
  - [x] Build a list of every affected file and line range
- [x] Audit `src/unilib/arch.c` — `apply_sandbox()` and `drop_privs()` (AC: #1)
  - [x] Read `drop_privs()` in full — confirm `setgroups(1, &mygid)` is called before `setuid()`/`setgid()`
  - [x] If missing: add `setgroups(1, &mygid)` in the correct position
  - [x] Verify no unvalidated buffer operations in `apply_sandbox()` or surrounding code
- [x] Audit `src/usignals.c` — SIGCHLD handler and `children_dead` (AC: #1)
  - [x] Confirm `children_dead` is `volatile sig_atomic_t` (see also Story 2.3)
  - [x] Confirm all signal handlers use only async-signal-safe functions
  - [x] Document any non-compliant function calls found
- [x] Audit `src/unilib/xpoll.c` — kqueue fd CLOEXEC handling (AC: #1)
  - [x] Confirm kqueue fd is created with `CLOEXEC` or has it set immediately after creation
  - [x] Assess whether the non-fatal fallback for CLOEXEC is acceptable; document rationale
- [x] Audit `src/unilib/intf.c` — `AF_LINK` code paths (AC: #1)
  - [x] Read all `AF_LINK` branches for buffer safety (fixed-size arrays, `strncpy` vs `strlcpy`, etc.)
  - [x] Document any unsafe buffer operation found
- [x] Audit `src/unilib/route.c` — macOS routing code paths (AC: #1)
  - [x] Read all macOS-specific routing code for similar issues
- [x] Produce findings document (AC: #1)
  - [x] For each finding: severity (low/medium/high), exact file:line, description of the problem, proposed fix, Chesterton's fence rationale
  - [x] Apply all fixes that are within scope; flag any that require a separate story

## Dev Notes

### Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it. Read the function. Read its callers. Read the history.

### Implementation Guidance

- Audit all code behind `#ifdef __APPLE__`, `HAVE_SANDBOX_H`, `AF_LINK`, `HAVE_KQUEUE`
- CFA Agent #7 found 10 findings, 3 medium: `children_dead` not sig_atomic_t, `setgroups()` missing, kqueue fd CLOEXEC non-fatal
- Follow Jack Louis style for all fixes
- Chesterton's fence for each finding: understand why the code was written that way

**Mantra:** Security audit means reading every line, not skimming. Every `#ifdef __APPLE__` block. No assumptions.

### Project Structure Notes

- `src/unilib/arch.c` — contains `apply_sandbox()` and `drop_privs()`; `setgroups()` call must be added before UID/GID drop
- `src/unilib/xpoll.c` — kqueue fd CLOEXEC handling to be audited
- `src/unilib/intf.c` — `AF_LINK` code paths to be audited for buffer safety
- `src/unilib/route.c` — macOS routing code paths to be audited
- `src/usignals.c` — `children_dead` must be changed to `volatile sig_atomic_t`; all signal handlers must use only async-signal-safe functions

### References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`

## Dev Agent Record

### Agent Model Used

claude-sonnet-4-6

### Debug Log References

None. All five changed files compiled cleanly. Pre-existing build failures (missing ltdl.h, --export-dynamic linker flag) are unrelated to this story.

### Completion Notes List

**Finding 1 — HIGH — src/unilib/arch.c: Missing setgroups() before privilege drop**

Severity: HIGH
Location: src/unilib/arch.c, drop_privs(), before the USE_SETRES/USE_SETRE/fallback block
Problem: drop_privs() called setresgid/setregid/setgid to change the GID but never called setgroups() to clear supplementary groups. A process started as root inherits supplementary groups (wheel, admin, etc.). setgid/setresgid only change the primary GID; supplementary groups remain intact. After the UID drop, the process still had supplementary group access to root-only resources accessible to those groups.
Fix: Added `setgroups(1, &mygid)` before the UID/GID drop sequence, with `#include <grp.h>` added to satisfy the declaration. The call replaces the entire supplementary group list with exactly the target GID.
Chesterton's Fence: The original code was written before macOS support was added. On Linux with SELinux (WITH_SELINUX), drop_privs() returns early (line 171-173), so this path was only reached in the non-SELinux case. The omission was an oversight, not a deliberate design choice — no comment justifies skipping setgroups().

**Finding 2 — NONE — src/usignals.c: children_dead already volatile sig_atomic_t**

Severity: N/A (already correct)
Location: src/usignals.c, lines 29-30
Problem: Story 2.3 was supposed to add volatile sig_atomic_t to children_dead. Audit found both children_synced and children_dead are already declared as `static volatile sig_atomic_t`. No action needed.
Signal handler safety: signals_chlddead() calls wait() which is async-signal-safe on POSIX. signals_chldsync() only increments the sig_atomic_t counter. Both handlers are compliant.
Chesterton's Fence: Story 2.3 already applied this fix. Nothing to change.

**Finding 3 — MEDIUM — src/unilib/intf.c: AF_LINK path missing sdl_nlen+sdl_alen bounds check**

Severity: MEDIUM
Location: src/unilib/intf.c, AF_LINK branch (~line 72-78)
Problem: The code cast pcapaddr_u.raw to struct sockaddr_dl * and checked sdl->sdl_alen == THE_ONLY_SUPPORTED_HWADDR_LEN before calling LLADDR(sdl) and memcpy. LLADDR(sdl) expands to sdl->sdl_data + sdl->sdl_nlen. If sdl_nlen + sdl_alen exceeded sdl->sdl_len minus the fixed header size, the memcpy would read beyond the sockaddr_dl. A malformed sockaddr_dl from a buggy kernel driver or corrupted pcap data could trigger an out-of-bounds read.
Fix: Added bounds check `(int)sdl->sdl_nlen + (int)sdl->sdl_alen <= (int)sdl->sdl_len - (int)offsetof(struct sockaddr_dl, sdl_data)` alongside the existing sdl_alen check. Also replaced the unnecessary cast via .raw with the cleaner .sdl union member.
Chesterton's Fence: The original check only validated sdl_alen (the hardware address length). This is sufficient for normal kernel-provided addresses but does not defend against malformed structures. The sdl_nlen validation was not there because the original code assumed well-formed kernel data. Added without removing the alen check.

**Finding 4 — MEDIUM — src/unilib/route.c: sysctl two-call pattern lacks ENOMEM retry**

Severity: MEDIUM
Location: src/unilib/route.c, get_netroutes() sysctl path (~lines 340-358)
Problem: The BSD/macOS sysctl routing table read used the standard two-call pattern: first call with NULL buffer to get size, second call with allocated buffer to fetch data. Between the two calls, the kernel routing table could grow (new routes added), causing the second call to return ENOMEM. The original code treated ENOMEM as a fatal error, causing get_netroutes() to return with no routing table populated, breaking all route lookups.
Fix: Added a retry loop (up to 3 retries) that detects ENOMEM on the second sysctl call, frees the buffer, re-reads the needed size from the kernel's updated *oldlenp output, adds 25% headroom, and retries. On non-ENOMEM errors, the original fatal path is preserved.
Chesterton's Fence: The original two-call pattern is standard BSD practice. The missing retry is a common oversight — most code assumes the routing table does not change between the two calls, which is usually true in a quiescent system but not guaranteed under load.

**Finding 5 — MEDIUM — src/unilib/xpoll.c: kqueue FD_CLOEXEC failure made non-fatal**

Severity: MEDIUM (was LOW due to non-fatal handling)
Location: src/unilib/xpoll.c, get_kqueue_fd(), after kqueue() call
Problem: After creating the kqueue fd, the code set FD_CLOEXEC via fcntl but treated failure as non-fatal, logging ERR and continuing. A persistent kqueue fd without CLOEXEC leaks into all forked child processes (unicornscan forks send and receive drone processes). A child with the inherited kqueue fd could theoretically monitor file descriptor activity on fds registered in the parent's kqueue, which is a security concern.
Fix: Changed the non-fatal fallback to a fatal error: on fcntl failure, close the kqueue fd, reset kq_fd to -1, and return -1. Added a detailed comment explaining the rationale. A freshly created kqueue fd should never fail fcntl(F_SETFD); failure indicates a kernel problem that warrants aborting rather than running in a degraded state.
Chesterton's Fence: The original non-fatal choice was pragmatic — avoid breaking the scan if FD_CLOEXEC is not critical. However, because unicornscan explicitly forks child processes and the kqueue fd is persistent (module-level static), the risk is real and the failure case is essentially impossible under normal conditions.

### Change Log

- src/unilib/arch.c: Added `#include <grp.h>`; added `setgroups(1, &mygid)` before UID/GID drop with comment explaining rationale
- src/unilib/intf.c: Added sdl_nlen+sdl_alen bounds check in AF_LINK path; replaced `.raw` cast with `.sdl` union member
- src/unilib/route.c: Added ENOMEM retry loop (up to 3 attempts, 25% headroom) to sysctl two-call pattern in get_netroutes()
- src/unilib/xpoll.c: Changed FD_CLOEXEC failure from non-fatal (ERR + continue) to fatal (close fd, reset, return -1)
- src/usignals.c: No change — children_dead already correctly declared as volatile sig_atomic_t (Story 2.3 already applied)

### File List

- src/unilib/arch.c
- src/unilib/intf.c
- src/unilib/route.c
- src/unilib/xpoll.c
