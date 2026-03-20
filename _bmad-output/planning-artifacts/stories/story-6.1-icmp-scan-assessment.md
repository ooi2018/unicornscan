# Story 6.1: ICMP Scan Mode Status Assessment

Status: review

## Story

As a developer,
I want to determine whether ICMP scanning works, is partial, or is a dead stub,
so that we can validate it or document it as unsupported.

## Acceptance Criteria

1. **Given** the MODE_ICMPSCAN code paths are audited **When** the assessment is complete **Then** a clear determination: working / partial / dead stub **And** if dead stub: recommendation to implement or remove scaffolding **And** GETTING_STARTED.md updated to reflect actual ICMP status

## Tasks / Subtasks

- [x] Read all ICMP-related source code before drawing any conclusions (AC: #1)
  - [x] Read `src/scan_progs/scanopts.c:474-563` — `scan_parsemode()` — check for presence or absence of an `'I'` branch
  - [x] Read `src/scan_progs/send_packet.c:527-562` — the workunit switch — check whether `ICMP_SEND_MAGIC` is handled
  - [x] Read `src/scan_progs/workunits.c` — ICMP workunit definitions — assess completeness
  - [x] Search for all references to `MODE_ICMPSCAN` (value 8) across the entire codebase
- [x] Record findings with exact file and line references (AC: #1)
  - [x] Confirm or deny: does `scan_parsemode()` have an `'I'` branch?
  - [x] Confirm or deny: is `ICMP_SEND_MAGIC` handled in the send switch?
  - [x] Classify the state: working / partial / dead stub — with evidence
- [x] Formulate a recommendation (AC: #1)
  - [x] If dead stub: recommend either implementing ICMP mode fully or removing the scaffolding
  - [x] If partial: identify exactly which pieces are missing
  - [x] If working: document which ICMP types are supported
- [x] Update `GETTING_STARTED.md` to reflect the actual ICMP status (AC: #1)
  - [x] If dead stub or partial: note ICMP as unsupported or experimental
  - [x] If working: document the correct usage flag and any privilege requirements
  - [x] Do NOT implement ICMP mode as part of this story — assessment and documentation only

## Dev Notes

### Mandatory Standards

All code MUST follow `docs/jack-louis-coding-style-guide.md`.

> **Mantra:** DO NOT BE LAZY. We have plenty of time to do it right. No shortcuts. Never make assumptions. Always dive deep and ensure you know the problem you're solving. Measure 3x, cut once. No fallback. No stub code. Just pure excellence.

> **Chesterton's Fence:** Always understand existing code fully before changing it. Read the function. Read its callers. Read the history.

### Implementation Guidance

- Read `src/scan_progs/scanopts.c:474-563` — does `scan_parsemode()` have an `'I'` branch?
- Read `src/scan_progs/send_packet.c:527-562` — does the workunit switch handle `ICMP_SEND_MAGIC`?
- CFA Agent #7 found: MODE_ICMPSCAN defined (value 8), magic numbers exist, but `scan_parsemode()` has NO `'I'` branch — dead stub
- Chesterton's fence: ICMP mode was likely planned but never finished in the original 0.4.7

**Mantra:** Do NOT implement ICMP mode in this story. This is ASSESSMENT ONLY. Determine exact state, document, recommend.

### Project Structure Notes

- `src/scan_progs/scanopts.c` — `scan_parsemode()` at lines 474-563; check whether an `'I'` branch exists
- `src/scan_progs/send_packet.c` — workunit switch at lines 527-562; check whether `ICMP_SEND_MAGIC` is handled
- `src/scan_progs/workunits.c` — ICMP workunit definitions to be assessed for completeness

### References

- PRD: `docs/PRD-v051-macos-hardening.md`
- Style Guide: `docs/jack-louis-coding-style-guide.md`
- Epics: `_bmad-output/planning-artifacts/epics.md`

## Dev Agent Record

### Agent Model Used

claude-sonnet-4-6

### Debug Log References

- `sudo /opt/homebrew/bin/unicornscan -mI 127.0.0.1` output recorded below in Completion Notes.

### Completion Notes List

#### ICMP Mode Determination: DEAD STUB

**Verdict: DEAD STUB**

ICMP scan mode (`-mI`) is non-functional. The mode constant and magic numbers
exist in the codebase but the command-line parser has no branch to activate the
mode. Attempting to invoke it fails immediately at argument parsing.

---

#### Evidence by Component

**1. `scan_parsemode()` — No 'I' branch (MISSING)**

File: `src/scan_progs/scanopts.c`, lines 474–563

The function handles these mode characters only:
- `'T'` → `MODE_TCPSCAN`
- `'U'` → `MODE_UDPSCAN`
- `'A'` → `MODE_ARPSCAN`
- `"sf"` → `MODE_TCPSCAN` with `M_DO_CONNECT`
- `"tr"` → `MODE_TCPTRACE`

There is no `'I'` branch. The same absence exists in `scan_parsemode_ext()`
(lines 598–746), which is the per-phase parser used by compound mode. Both
functions fall through to the `else` clause that emits:
```
unknown scanning mode ' '
```
This is confirmed by running `unicornscan -mI 127.0.0.1`:
```
Main [Error   scanopts.c:562] unknown scanning mode ' '
```
(The error prints `str[1]` instead of `str[0]`; the character seen is a space
because 'I' is at index 0, so `str[1]` is the next char — a minor pre-existing
formatting bug, unrelated to the stub issue.)

**2. `send_packet.c` workunit switch — No `ICMP_SEND_MAGIC` case (MISSING)**

File: `src/scan_progs/send_packet.c`, lines 527–562

The sender's workunit dispatch switch handles:
- `TCP_SEND_MAGIC` (0x1a1b1c1d)
- `TCPTRACE_SEND_MAGIC` (0x8a8b8c8d)
- `UDP_SEND_MAGIC` (0x2a2b2c2d)
- `ARP_SEND_MAGIC` (0x3a3b3c3d)

`ICMP_SEND_MAGIC` (0x4a4b4c4d) is **absent**. If the parser bug were fixed and
an ICMP workunit were somehow created, the sender would log:
```
unknown workunit type 0x4a4b4c4d, ignoring
```
and skip the workunit entirely. No ICMP packet would be transmitted.

**3. `workunits.c` / `workunits.h` — Magic numbers defined but unreachable**

File: `src/scan_progs/workunits.h`, lines 25 and 34

Both magic numbers are defined:
```c
#define ICMP_SEND_MAGIC 0x4a4b4c4d
#define ICMP_RECV_MAGIC 0xa4b4c4d4
```

In `workunit_add()` (`src/scan_progs/workunits.c`, lines 500–503), there is a
`case MODE_ICMPSCAN:` branch that correctly assigns both magic numbers:
```c
case MODE_ICMPSCAN:
    lwu_srch.magic=ICMP_RECV_MAGIC;
    send_magic=ICMP_SEND_MAGIC;
    break;
```

This code is correct scaffolding but dead: because `scan_parsemode()` cannot
set `mode = MODE_ICMPSCAN`, this `case` is never reached via the CLI.

**4. `packet_parse.c` — Receive-side partially wired**

File: `src/scan_progs/packet_parse.c`, line 320

`MODE_ICMPSCAN` is grouped with the IP decode path:
```c
case MODE_TCPSCAN:
case MODE_TCPTRACE:
case MODE_UDPSCAN:
case MODE_ICMPSCAN:
case MODE_IPSCAN:
    report_init(REPORT_TYPE_IP, &phdr->ts);
    decode_ip(packet, pk_len, pk_layer);
    break;
```
This means the listener would decode received ICMP packets through the generic
IP decoder if the mode were ever active. However, there is no ICMP-specific
response analysis (e.g., echo-reply correlation, ICMP type/code filtering). The
listener would see whatever the IP decoder surfaces, which is not purpose-built
for ICMP echo scanning.

**5. `scan_export.h` — Constant defined**

File: `src/scan_progs/scan_export.h`, line 53:
```c
#define MODE_ICMPSCAN 8
```
Defined alongside all other mode constants. Not the problem.

**6. `pgsqldb.c` — Database layer aware of mode 8**

File: `src/output_modules/database/pgsqldb.c`, line 68:
```c
case 8:  return 'I'; /* MODE_ICMPSCAN */
```
The database output module translates mode 8 back to 'I'. This is consistent
scaffolding, not functional capability.

---

#### Summary of What Is Present vs Missing

| Component | Present | Functional |
|---|---|---|
| `MODE_ICMPSCAN` constant (value 8) | Yes | N/A |
| `ICMP_SEND_MAGIC` / `ICMP_RECV_MAGIC` | Yes | N/A |
| `workunits.c`: `case MODE_ICMPSCAN` magic assignment | Yes | Dead (unreachable) |
| `packet_parse.c`: `case MODE_ICMPSCAN` receive path | Yes | Dead (unreachable) |
| `pgsqldb.c`: mode 8 → 'I' mapping | Yes | Dead (unreachable) |
| `scan_parsemode()`: `'I'` parser branch | **NO** | **MISSING** |
| `scan_parsemode_ext()`: `'I'` parser branch | **NO** | **MISSING** |
| `send_packet.c`: `ICMP_SEND_MAGIC` sender case | **NO** | **MISSING** |
| ICMP echo packet construction in sender | **NO** | **MISSING** |
| ICMP echo-reply correlation in listener | **NO** | **MISSING** |

---

#### What Would Be Required to Implement ICMP Mode

Minimum work to reach a functional ICMP echo ping-sweep:

1. **`scanopts.c` — `scan_parsemode()` and `scan_parsemode_ext()`**: Add `'I'`
   branch setting `*mode = MODE_ICMPSCAN`. Low complexity.

2. **`send_packet.c` — workunit switch**: Add `case ICMP_SEND_MAGIC:` that calls
   `open_link(SOCK_IP, ...)`, sets `s->ss->mode = MODE_ICMPSCAN`, and
   handles the per-host send loop. Moderate complexity: requires writing an
   ICMP echo request packet builder using `myicmphdr` from `packets.h`, with
   a unique `id`/`sequence` per probe for correlation.

3. **Listener ICMP echo-reply correlation**: The generic `decode_ip()` path
   would see ICMP echo-reply but has no mechanism to correlate reply `id`/`seq`
   back to a sent probe. A dedicated correlator analogous to the TCP SYN-cookie
   trick would be needed. Moderate-to-high complexity.

4. **Port string semantics**: ICMP mode has no ports. The workunit scaffolding
   (e.g., `port_str` handling, `parse_pstr()` for packet counting) would need
   bypassing, similar to ARP mode's `num_pkts = 1` path. Low complexity.

5. **pcap filter string**: The receiver needs an ICMP-specific filter (e.g.,
   `icmp[icmptype] == icmp-echoreply`). Low complexity.

**Estimated effort**: 2–4 days for a competent C developer familiar with the
codebase, to produce a basic working ICMP echo sweep with proper correlation.
A production-quality implementation with ICMP type selection (timestamp, mask,
etc.) would be 1–2 weeks.

---

#### Recommendation

**Defer implementation; leave scaffolding in place.**

Rationale:
1. The scaffolding (magic numbers, mode constant, workunit case, database
   mapping) is consistent and well-placed. It costs nothing to leave it.
2. ICMP ping sweep is already available via OS tools (`ping`, `fping`, `nmap -sn`).
   The primary value of ICMP in unicornscan would be as part of compound mode
   (e.g., `-mI+T`) for host discovery without ARP — useful only on non-Ethernet
   or routed segments. That use case is not critical for the current roadmap.
3. Removing scaffolding now only to re-add it later (if ICMP is ever prioritized)
   would be wasteful churn.
4. The documentation now reflects the actual status (unsupported), so no user
   confusion should result from leaving the dead code in place.

**Do not remove scaffolding. Do not implement now. Track as a future enhancement.**

### Change Log

- `docs/GETTING_STARTED.md`: Added note to Quick Reference table that `-mI`
  (ICMP mode) is not yet implemented; users attempting it will receive an error.

### File List

- `docs/GETTING_STARTED.md` (updated: ICMP status note in Quick Reference table)
