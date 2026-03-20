# PRD: macOS First-Class Platform Support

**Version:** 1.1
**Date:** 2026-03-20
**Author:** John (PM) with full BMAD team analysis
**Target Release:** v0.5.0
**Status:** Implemented (see Section 11 for completion status)

---

## 1. Overview

### 1.1 Problem Statement

Unicornscan is an asynchronous, stateless network reconnaissance scanner that was originally built for Linux (circa 2004) and recently modernized (2025-2026). As of v0.5.0, macOS Apple Silicon is a first-class supported platform with full scanning parity, native privilege management via ChmodBPF, kqueue event loop, Homebrew formula, and DMG installer.

This PRD documents the requirements, technical decisions, and implementation status of the macOS port. It serves as the authoritative reference for what was built, what was deferred, and lessons learned during the port.

### 1.2 Objective

Make macOS a **first-class supported platform** with full scanning parity, native privilege management, Alicorn web UI support, and two distribution channels (Homebrew and DMG installer). The result is a macOS experience that matches or exceeds the current Linux experience.

### 1.3 Target Release

**v0.5.0** — a minor version bump signaling new platform support.

---

## 2. Scope

### 2.1 In Scope

- Full scanner functionality on macOS (TCP SYN, UDP, ARP, ICMP scanning)
- Apple Silicon (M1/M2/M3/M4) — arm64 architecture only
- macOS 13 Ventura and later
- Homebrew formula distribution
- DMG installer distribution
- ChmodBPF-style privilege management (LaunchDaemon)
- macOS sandbox profiles (replacing Linux chroot)
- kqueue event loop (replacing poll() on macOS)
- macOS `.app` launcher for Docker-based Alicorn
- Cross-platform `unicornscan-alicorn` management script
- macOS ARM64 build job in release CI workflow
- Hybrid file layout (Homebrew prefix for brew, Apple conventions for DMG)

### 2.2 Out of Scope

- Windows / WSL support
- Intel Mac (x86_64-apple-darwin) support
- x86 exploit payloads (`httpexp.c` shellcode) on ARM64
- Native Alicorn desktop app with embedded database (Phase 2)
- PR-level CI workflow (separate initiative)

### 2.3 Future Phase (Phase 2)

- Native Tauri/Electron Alicorn app with embedded PostgreSQL/SQLite
- macOS code signing and notarization for Gatekeeper
- App auto-update mechanism via Sparkle framework

---

## 3. Technical Analysis

### 3.1 Current State Assessment

A comprehensive file-by-file audit of ~85 source files identified the following categories of changes required:

| Category | Files Affected | Estimated Lines Changed |
|----------|---------------|------------------------|
| Build system (configure.ac, Makefiles) | 5 | ~80 |
| Network / packet handling | 4 | ~100 |
| Process / privilege / sandbox | 3 | ~60 |
| Event loop (kqueue) | 1 | ~80 |
| Modules and tools | 3 | ~30 |
| Scripts and packaging | 4 | ~150 |
| New files (Homebrew formula, LaunchDaemon, sandbox profile, .app) | ~8 | ~400 |
| **Total** | **~28** | **~900** |

### 3.2 What Already Works

The codebase has substantial portable foundations:

| Component | File(s) | Status |
|-----------|---------|--------|
| Packet structures (endian-aware) | `packets.h` | Fully portable |
| Checksum computation | `chksum.c` | Fully portable (RFC1071) |
| Drone IPC communication | `drone.c` | Fully portable (TCP sockets) |
| IPC mechanism | `xipc.c` | Fully portable (Unix domain sockets) |
| Signal handling | `usignals.c` | Fully portable (POSIX signals) |
| BPF immediate mode | `pcaputil.c` | Already conditional on `BIOCIMMEDIATE` |
| Offload disable stubs | `pcaputil.c` | Already `#ifdef __linux__` guarded with macOS stubs |
| `sockaddr.sa_len` handling | `unicorn_defs.h` | Properly conditionalized |
| `netpacket/packet.h` guard | `unicorn_defs.h` | Properly `#ifdef`'d |
| OS fingerprint database | `p0f3_parse.c` | Includes macOS/iOS signatures |
| Payload modules (12 of 14) | `src/payload_modules/` | Protocol-level, platform-agnostic |
| PostgreSQL output | `pgsqldb.c` | Standard libpq, portable |
| All tools except fantaip | `unibrow.c`, `fpdb.c`, `unicfgtst.c` | Portable |

---

## 4. Requirements

### 4.1 Epic 1: Build System — Darwin Support

**Goal:** Unicornscan compiles cleanly on macOS with `./configure && make`.

#### Story 1.1: configure.ac Darwin Case
**Priority:** P0 (Blocker)
**File:** `configure.ac:229-230`

The Darwin host case is empty. Add:
- `AC_DEFINE([USE_SETRE], [1], [use setreuid/setregid for privilege dropping])` — Darwin supports `setreuid`/`setregid`
- Detection for macOS sandbox APIs (`sandbox_init`)
- Detection for kqueue availability

**Acceptance Criteria:**
- `./configure` on macOS detects Darwin and sets appropriate defines
- Privilege dropping mechanism is selected automatically
- `config.h` contains `USE_SETRE`, `HAVE_KQUEUE`, and `HAVE_SANDBOX_INIT` on macOS

#### Story 1.2: Homebrew Dependency Detection
**Priority:** P0 (Blocker)
**File:** `configure.ac:271-294`

Library search paths are hardcoded to Linux multiarch (`/usr/lib/x86_64-linux-gnu`). Add:
- `/opt/homebrew/opt/*/lib` and `/opt/homebrew/opt/*/include` (Apple Silicon Homebrew)
- `/usr/local/opt/*/lib` and `/usr/local/opt/*/include` (Intel Homebrew — for source builders)
- pkg-config as primary detection method where available

**Affected Libraries:**
- libpcap
- libdnet
- libpq (PostgreSQL)
- libmaxminddb (GeoIP)
- libltdl

**Acceptance Criteria:**
- `./configure --with-pgsql` finds Homebrew-installed PostgreSQL on Apple Silicon
- All optional libraries detected via Homebrew paths
- `./configure` summary shows all found libraries

#### Story 1.3: ARM64 Compiler Flags
**Priority:** P0 (Blocker)
**File:** `configure.ac:215-217`

`-fPIC` is only added for `x86_64`. Extend to `aarch64`/`arm64` for shared library / dynamic module support.

**Acceptance Criteria:**
- `-fPIC` included in CFLAGS on Apple Silicon builds
- All `.dylib` modules compile and load correctly

#### Story 1.4: Conditional libdnet Patches
**Priority:** P1
**File:** `libs/Makefile.in:~19`

The bundled libdnet build applies Linux-specific patches (`libdnet-tun-linux-uio.patch`) unconditionally. These may break macOS compilation.

**Acceptance Criteria:**
- Linux-specific patches only applied when `uname` = Linux
- Bundled libdnet compiles on macOS (or system libdnet is used via Homebrew)

#### Story 1.5: macOS setcap Alternative
**Priority:** P1
**File:** `Makefile.in:55-83`

The `setcap` make target uses Linux capabilities. On macOS, this should either skip or provide guidance.

**Acceptance Criteria:**
- `make setcap` on macOS prints instructions for ChmodBPF setup instead of failing
- Linux behavior unchanged

---

### 4.2 Epic 2: Network Layer — macOS Compatibility

**Goal:** All scan modes (TCP SYN, UDP, ARP, ICMP) function correctly on macOS.

#### Story 2.1: Interface Enumeration — AF_LINK Support
**Priority:** P0 (Blocker)
**File:** `src/unilib/intf.c:30-111`

Hardware address detection uses `AF_PACKET` and `sockaddr_ll` (Linux-only). macOS uses `AF_LINK` with `sockaddr_dl`.

**Implementation:**
```c
#ifdef AF_PACKET
    if (pcapaddr_u.fs->family == AF_PACKET) {
        memcpy(ii->hwaddr, pcapaddr_u.sl->sll_addr, THE_ONLY_SUPPORTED_HWADDR_LEN);
    }
#endif
#ifdef AF_LINK
    if (pcapaddr_u.fs->family == AF_LINK) {
        struct sockaddr_dl *sdl = (struct sockaddr_dl *)pcapaddr_u.fs;
        memcpy(ii->hwaddr, LLADDR(sdl), THE_ONLY_SUPPORTED_HWADDR_LEN);
    }
#endif
```

**Acceptance Criteria:**
- `unicornscan -I` correctly identifies interface hardware addresses on macOS
- Both en0 (WiFi) and en* (Ethernet/Thunderbolt) interfaces detected

#### Story 2.2: Routing Table — BSD sysctl Integration
**Priority:** P1
**File:** `src/unilib/route.c:113-199`, `src/unilib/bsd-route.c`

Route discovery reads `/proc/net/route` (Linux). The fallback to `pcap_lookupdev()` works but is deprecated. A proper BSD implementation already exists in `bsd-route.c` using `sysctl()` with `CTL_NET, PF_ROUTE, NET_RT_DUMP`.

**Acceptance Criteria:**
- macOS uses sysctl-based routing (from bsd-route.c)
- Default gateway and interface correctly identified
- Linux behavior unchanged (continues using `/proc/net/route`)

#### Story 2.3: Packet Reception — pcap_setdirection Guard
**Priority:** P0 (Blocker)
**File:** `src/scan_progs/recv_packet.c:284-285`

`pcap_setdirection(PCAP_D_IN)` is called inside an `#ifdef PCAP_D_IN` block but the guard doesn't protect correctly on all macOS libpcap versions.

**Acceptance Criteria:**
- Compiles without error on macOS
- Packet reception functions correctly without direction filtering
- No false positive scan results from seeing own outbound packets

#### Story 2.4: Packet Injection — libdnet/BPF Verification
**Priority:** P0 (Blocker)
**Files:** `src/scan_progs/send_packet.c:1260-1279`, `src/tools/fantaip.c`
**Status:** Done (with critical BPF contention fix)

Packet injection uses `ip_open()` (IP layer) and `eth_open()` (link layer) via libdnet. On macOS, libdnet's BPF backend uses `/dev/bpf*` devices.

**Critical discovery — BPF device contention:**
Homebrew's libdnet 1.18.2 hardcodes `/dev/bpf0` in its `eth_open()` implementation (confirmed via `strings libdnet.1.dylib`). If any other process (pcap, unilisten, another scanner) holds `/dev/bpf0`, `eth_open()` fails with `EBUSY`. This was verified with a standalone test program that tested both open orders.

**Fix applied to fantaip.c:** Reorder `eth_open()` before `pcap_open_live()`. libdnet grabs `/dev/bpf0` first, pcap falls through to `/dev/bpf1`. Also fixed pcap timeout from `-1` (causes `BIOCSRTIMEOUT` on macOS BPF) to `100` ms.

**Coexistence verified:** fantaip + unicornscan running simultaneously on the same interface works correctly — both exit cleanly with no stale processes.

**Known limitation:** libdnet's `/dev/bpf0` hardcoding means only one libdnet-using process can run at a time unless other BPF consumers start first and leave `bpf0` free. Future fix: patch libdnet to iterate `/dev/bpf0` through `/dev/bpf255` (like pcap does), or replace libdnet's eth layer with native BPF.

**Acceptance Criteria:**
- [x] TCP SYN scan completes successfully on macOS
- [x] UDP scan completes successfully on macOS
- [x] ARP scan completes successfully on macOS (with appropriate privileges)
- [x] fantaip runs without BPF contention errors
- [x] fantaip + unicornscan coexist simultaneously

#### Story 2.5: Fix ntalk.c htons Bug
**Priority:** P1
**File:** `src/payload_modules/ntalk.c:90-94`

`htons(AF_INET)` is applied to `sin_family` — this is incorrect on all platforms. `sin_family` should not be byte-swapped.

**Implementation:**
```c
// Before (broken):
ret->src.sin_family = htons(AF_INET);
// After (correct):
ret->src.sin_family = AF_INET;
```

**Acceptance Criteria:**
- ntalk payload generates correct protocol family field
- Fix applied to all four `sin_family` assignments (lines 90-94)

---

### 4.3 Epic 3: Privilege Model — ChmodBPF LaunchDaemon

**Goal:** Users can run unicornscan for raw packet operations without sudo after initial setup.

#### Story 3.1: ChmodBPF LaunchDaemon
**Priority:** P0
**New files:**
- `macos/org.unicornscan.ChmodBPF.plist` (LaunchDaemon)
- `macos/ChmodBPF` (shell script)

Create a LaunchDaemon (similar to Wireshark's approach) that:
1. Creates a `unicornscan` group on the system
2. On boot, sets `/dev/bpf*` devices to be readable/writable by the `unicornscan` group
3. Adds the installing user to the `unicornscan` group

**Acceptance Criteria:**
- After installation and reboot, BPF devices are accessible to group members
- `unicornscan` runs raw scans without sudo
- `sudo unicornscan` still works as fallback
- Graceful degradation: without ChmodBPF, scanner detects permission issue and suggests setup

#### Story 3.2: macOS Sandbox Profile
**Priority:** P1
**New file:** `macos/unicornscan-listener.sb` (sandbox profile — written but not loadable)
**Modified file:** `src/unilib/arch.c`
**Status:** Deferred — sandbox profile exists but cannot be applied via `sandbox_init()`

**Original plan:** Replace `chroot()` on macOS with `sandbox_init()` using a custom `.sb` profile.

**What we discovered:** Apple's `sandbox_init()` API was deprecated in macOS 10.8 and the `SANDBOX_NAMED` flag only accepts Apple's built-in profile constants (`kSBXProfileNoNetwork`, etc.), not inline Scheme profile text or file paths. Testing on macOS 26.x confirmed that passing custom profile content via `SANDBOX_NAMED` fails with "profile not found". `SANDBOX_NAMED_EXTERNAL` is not defined in modern SDKs.

**Current implementation:** `apply_sandbox()` in `arch.c` logs that sandboxing is unavailable and returns 0 (non-fatal). The listener process relies on the existing non-root privilege model (ChmodBPF group access) rather than process-level sandboxing. The `.sb` profile file exists in `macos/` for reference.

**Future options:**
- Use `sandbox-exec -f profile.sb` at the `execve()` callsite in `chld.c`
- Investigate App Sandbox entitlements (requires code signing)
- Accept that ChmodBPF + non-root operation provides sufficient isolation

**Acceptance Criteria (revised):**
- [x] Build system detects `sandbox_init` via `HAVE_SANDBOX_INIT`
- [x] `apply_sandbox()` fails gracefully without error spam
- [x] Listener process runs inside chroot on Linux (unchanged)
- [ ] ~~Listener process runs inside macOS sandbox~~ (deferred)

---

### 4.4 Epic 4: Event Loop — kqueue Support

**Goal:** macOS builds use kqueue for optimal I/O event handling.

#### Story 4.1: kqueue Event Loop Implementation
**Priority:** P2
**File:** `src/unilib/xpoll.c:35-79`

Current implementation uses POSIX `poll()`. Add kqueue backend for macOS:

**Implementation approach:**
```c
#ifdef HAVE_KQUEUE
// kqueue-based implementation
int xpoll(xpoll_t *array, uint32_t len, int timeout) {
    struct kevent events[MAX_CONNS];
    // Use kqueue for event notification
}
#else
// Existing poll()-based implementation (Linux, fallback)
#endif
```

**Acceptance Criteria:**
- macOS builds use kqueue for I/O multiplexing
- Linux builds continue using poll() (unchanged)
- Functional behavior identical across platforms
- Performance improvement measurable on macOS (reduced syscall overhead)

---

### 4.5 Epic 5: Alicorn — macOS Support

**Goal:** Alicorn web UI runs on macOS with Docker, managed by a cross-platform script, with a `.app` launcher.

#### Story 5.1: Cross-Platform Management Script
**Priority:** P0
**File:** `debian/unicornscan-alicorn` (573 lines)

Unify into a single cross-platform bash script with OS detection:

| Linux Command | macOS Equivalent |
|---------------|------------------|
| `ss -tuln \| grep :PORT` | `lsof -i :PORT` |
| `systemctl` | N/A (Docker manages lifecycle) |
| `setcap` | ChmodBPF reference |
| `groupadd --system` | `dseditgroup -o create` |
| `usermod -aG` | `dseditgroup -o edit -a` |
| `/var/lib/unicornscan/alicorn` | `$HOMEBREW_PREFIX/var/unicornscan/alicorn` (brew) or `~/Library/Application Support/unicornscan/alicorn` (DMG) |

**Acceptance Criteria:**
- `unicornscan-alicorn start/stop/status/logs/shell/reset` works on both Linux and macOS
- One script file, OS-detected branches where needed
- Password generation, Docker health checks, and container management work on both platforms
- GeoIP paths resolve correctly on macOS

#### Story 5.2: GeoIP Update Script — macOS Paths
**Priority:** P1
**File:** `scripts/unicornscan-geoip-update`

Update GeoIP database paths and search logic:

| Linux Path | macOS Path |
|------------|------------|
| `/usr/share/GeoIP` | `$HOMEBREW_PREFIX/share/GeoIP` (brew) or `/usr/local/share/GeoIP` |

**Acceptance Criteria:**
- `unicornscan-geoip-update` downloads and installs databases on macOS
- Correct paths used for both Homebrew and DMG installations
- Scanner config updated with correct macOS database paths

#### Story 5.3: macOS `.app` Launcher
**Priority:** P2
**New files:** `macos/Alicorn.app/` bundle structure

A lightweight macOS application bundle that:
- Checks Docker Desktop is running (prompts to start if not)
- Calls `unicornscan-alicorn start` to launch the Docker stack
- Opens `http://localhost:31337` in the default browser
- Lives in the menu bar with status indicator
- Provides Stop/Restart/Logs from menu bar dropdown

**Acceptance Criteria:**
- Double-clicking Alicorn.app starts the web UI stack
- Menu bar icon shows running/stopped status
- Works with both Docker Desktop and colima

---

### 4.6 Epic 6: Distribution & Packaging

**Goal:** Users can install unicornscan via Homebrew or DMG on macOS.

#### Story 6.1: Homebrew Formula
**Priority:** P0
**New file:** `macos/unicornscan.rb` (Homebrew formula)

```ruby
class Unicornscan < Formula
  desc "Asynchronous stateless TCP/UDP network scanner"
  homepage "https://github.com/robertelee78/unicornscan"
  url "https://github.com/robertelee78/unicornscan/archive/refs/tags/v0.5.0.tar.gz"
  license "GPL-2.0-or-later"

  depends_on "libpcap"
  depends_on "libdnet"
  depends_on "libtool"
  depends_on "postgresql@16" => :optional
  depends_on "libmaxminddb" => :optional
  depends_on "flex" => :build
  depends_on "bison" => :build
  depends_on "autoconf" => :build
  depends_on "automake" => :build

  def install
    system "autoreconf", "-fiv"
    system "./configure", *std_configure_args, "--with-pgsql"
    system "make", "-j#{ENV.make_jobs}"
    system "make", "install"
    # Install Alicorn files, management scripts, LaunchDaemon, etc.
  end
end
```

**Acceptance Criteria:**
- `brew install unicornscan` completes successfully on Apple Silicon
- Scanner binary, modules, config files, and management scripts all installed
- `brew test unicornscan` passes basic functionality check
- `brew uninstall unicornscan` cleanly removes everything

#### Story 6.2: DMG Installer
**Priority:** P1
**New files:** `macos/dmg/` build scripts

Create a `.dmg` disk image containing:
- Pre-compiled scanner binaries (arm64)
- Alicorn.app launcher
- ChmodBPF installer package
- README with quick start guide
- Drag-to-Applications installation

**File Layout (DMG install):**
```
/Applications/Alicorn.app
/usr/local/bin/unicornscan
/usr/local/bin/fantaip
/usr/local/bin/unicornscan-alicorn
/usr/local/bin/unicornscan-geoip-update
/usr/local/libexec/unicornscan/unisend
/usr/local/libexec/unicornscan/unilisten
/usr/local/lib/unicornscan/modules/*.dylib
/usr/local/etc/unicornscan/unicorn.conf
/usr/local/etc/unicornscan/modules.conf
/usr/local/etc/unicornscan/payloads.conf
/Library/LaunchDaemons/org.unicornscan.ChmodBPF.plist
~/Library/Application Support/unicornscan/alicorn/ (runtime data)
```

**Acceptance Criteria:**
- DMG mounts and displays install instructions
- Installation completes without errors
- Scanner runs after installation
- Uninstallation instructions provided

#### Story 6.3: Release Workflow — macOS Build Job
**Priority:** P0
**File:** `.github/workflows/release.yml`

Add a `build-macos` job to the existing release workflow:

```yaml
build-macos:
  name: Build macOS (ARM64)
  runs-on: macos-14  # Apple Silicon runner
  steps:
    - name: Install dependencies
      run: brew install libpcap libdnet libtool postgresql@16 libmaxminddb flex bison
    - name: Configure and build
      run: |
        autoreconf -fiv
        ./configure --prefix=/opt/homebrew --with-pgsql
        make -j$(sysctl -n hw.ncpu)
    - name: Create DMG
      run: # DMG creation script
    - name: Create Homebrew bottle
      run: # Bottle creation
```

**Acceptance Criteria:**
- Release workflow produces macOS ARM64 artifacts alongside existing Linux packages
- DMG and Homebrew bottle uploaded to GitHub Release
- Build succeeds on `macos-14` GitHub Actions runner

---

### 4.7 Epic 7: GeoIP Module — macOS Paths

**Goal:** GeoIP lookups work out of the box on macOS.

#### Story 7.1: Expand GeoIP Search Paths
**Priority:** P1
**Files:** `src/output_modules/database/geoip_ip2location.c`, `geoip_maxmind.c`

Add macOS-standard paths to the GeoIP database search arrays:

```c
static const char *search_paths[] = {
    "/usr/share/GeoIP",
    "/var/lib/GeoIP",
    "/usr/local/share/GeoIP",
    "/opt/homebrew/share/GeoIP",      // Apple Silicon Homebrew
    "/opt/homebrew/var/GeoIP",        // Alternative Homebrew
    "/opt/GeoIP",
    NULL
};
```

**Acceptance Criteria:**
- GeoIP databases found automatically after `brew install` or `unicornscan-geoip-update`
- No manual configuration required on macOS

---

## 5. Technical Change Map

### 5.1 Complete File-Level Change List

| # | File | Lines | Change | Epic | Priority |
|---|------|-------|--------|------|----------|
| 1 | `configure.ac` | 229-230 | Add `USE_SETRE`, sandbox, kqueue detection for Darwin | E1 | P0 |
| 2 | `configure.ac` | 271-294 | Add Homebrew library search paths | E1 | P0 |
| 3 | `configure.ac` | 215-217 | Extend `-fPIC` to arm64 | E1 | P0 |
| 4 | `src/unilib/intf.c` | 61 | Add `AF_LINK` / `sockaddr_dl` path | E2 | P0 |
| 5 | `src/scan_progs/recv_packet.c` | 284-285 | Properly guard `pcap_setdirection()` | E2 | P0 |
| 6 | `src/scan_progs/send_packet.c` | 1260-1279 | Verify/fix libdnet BPF backend on macOS | E2 | P0 |
| 7 | `src/scan_progs/send_packet.c` | 902-914 | IP header byte order for macOS raw sockets | E2 | P0 |
| 8 | `src/unilib/route.c` | 113-199 | Integrate bsd-route.c sysctl routing | E2 | P1 |
| 9 | `src/unilib/arch.c` | 184-192 | macOS sandbox_init() replacing chroot() | E3 | P1 |
| 10 | `src/unilib/xpoll.c` | 35-79 | Add kqueue backend | E4 | P2 |
| 11 | `src/payload_modules/ntalk.c` | 90-94 | Fix htons(AF_INET) bug | E2 | P1 |
| 12 | `src/output_modules/database/geoip*.c` | search_paths | Add Homebrew GeoIP paths | E7 | P1 |
| 13 | `Makefile.in` | 55-83 | Conditional setcap (Linux only) | E1 | P1 |
| 14 | `libs/Makefile.in` | ~19 | Conditional libdnet patches (Linux only) | E1 | P1 |
| 15 | `configure.ac` | 315-320 | Platform-aware p0f socket path | E1 | P2 |
| 16 | `debian/unicornscan-alicorn` | entire | Make cross-platform (OS detection) | E5 | P0 |
| 17 | `scripts/unicornscan-geoip-update` | 17+ | macOS path support | E5 | P1 |

### 5.2 New Files Required

| # | File | Purpose | Epic | Priority |
|---|------|---------|------|----------|
| 18 | `macos/org.unicornscan.ChmodBPF.plist` | LaunchDaemon for BPF permissions | E3 | P0 |
| 19 | `macos/ChmodBPF` | Shell script called by LaunchDaemon | E3 | P0 |
| 20 | `macos/unicornscan.sb` | macOS sandbox profile | E3 | P1 |
| 21 | `macos/unicornscan.rb` | Homebrew formula | E6 | P0 |
| 22 | `macos/dmg/build-dmg.sh` | DMG creation script | E6 | P1 |
| 23 | `macos/Alicorn.app/` | macOS app launcher bundle | E5 | P2 |
| 24 | `.github/workflows/release.yml` | Add macOS build job (modify existing) | E6 | P0 |

---

## 6. Dependencies

### 6.1 macOS System Dependencies

| Dependency | Source | Required | Purpose |
|------------|--------|----------|---------|
| Xcode Command Line Tools | Apple | Yes | Compiler (clang), headers |
| libpcap | System / Homebrew | Yes | Packet capture |
| libdnet | Homebrew | Yes | Packet injection |
| libtool (libltdl) | Homebrew | Yes | Dynamic module loading |
| flex | Homebrew | Build only | Parser generation |
| bison | Homebrew | Build only | Parser generation |
| autoconf | Homebrew | Build only | Build system |
| automake | Homebrew | Build only | Build system |
| postgresql@16 | Homebrew | Optional | Alicorn database export |
| libmaxminddb | Homebrew | Optional | GeoIP lookups |
| Docker Desktop | Docker Inc | Optional | Alicorn web UI |

### 6.2 Build Prerequisites

```bash
# Install Xcode CLI tools
xcode-select --install

# Install Homebrew dependencies
brew install autoconf automake libtool libpcap libdnet flex bison

# Optional
brew install postgresql@16 libmaxminddb
```

---

## 7. Risks and Mitigations

| Risk | Impact | Probability | Mitigation | Outcome |
|------|--------|-------------|------------|---------|
| libdnet macOS BPF backend insufficient for all scan modes | High | Medium | Test early in Epic 2; fallback to direct BPF | **Materialized** — libdnet hardcodes `/dev/bpf0`. Fixed by reordering `eth_open` before `pcap_open_live` in fantaip. unicornscan's send_packet uses `ip_open` which works. |
| IP header byte order differences cause silent scan failures | High | Medium | Dedicated test suite | Not observed — scans produce correct results |
| macOS sandbox profile too restrictive for scanner operation | Medium | Low | Iterative sandbox profile development | **Materialized differently** — `sandbox_init()` API is broken on modern macOS (deprecated since 10.8, `SANDBOX_NAMED` rejects custom profiles). Sandboxing deferred entirely. |
| Apple Silicon Homebrew path changes in future macOS versions | Low | Low | Use `$(brew --prefix)` dynamically | `configure.ac` uses dynamic `brew --prefix` detection |
| GitHub Actions macOS runners are expensive (10x Linux cost) | Medium | Certain | Limit to release builds only | Release-only macOS builds implemented |
| ChmodBPF approach may not survive macOS updates | Medium | Low | Monitor Wireshark's approach | No issues observed on macOS 26.x |
| pcap timeout `-1` invalid on macOS BPF | Medium | N/A | N/A (discovered during testing) | **Discovered** — `BIOCSRTIMEOUT` error. Fixed: timeout changed to `100` ms in fantaip. recv_packet.c already used `100`. |

---

## 8. Implementation Order

### Phase 1: Core Port (P0 items)
**Goal:** Scanner compiles and runs basic scans on macOS.

1. `configure.ac` Darwin case + Homebrew paths + arm64 flags (Stories 1.1, 1.2, 1.3)
2. `intf.c` AF_LINK support (Story 2.1)
3. `recv_packet.c` pcap_setdirection guard (Story 2.3)
4. `send_packet.c` libdnet/BPF verification (Story 2.4)
5. ChmodBPF LaunchDaemon (Story 3.1)
6. Cross-platform Alicorn management script (Story 5.1)
7. Homebrew formula (Story 6.1)
8. Release workflow macOS job (Story 6.3)

**Validation:** TCP SYN scan against a known target produces correct results on macOS.

### Phase 2: Full Functionality (P1 items)
**Goal:** All features work, packaging complete.

9. Route table integration (Story 2.2)
10. ntalk.c bug fix (Story 2.5)
11. macOS sandbox profile (Story 3.2)
12. GeoIP path expansion (Story 7.1)
13. GeoIP update script macOS support (Story 5.2)
14. Conditional libdnet patches + setcap alternative (Stories 1.4, 1.5)
15. DMG installer (Story 6.2)

**Validation:** Full scan suite including UDP, ARP, ICMP modes. Alicorn web UI operational via Docker.

### Phase 3: Polish (P2 items)
**Goal:** Optimized, polished macOS experience.

16. kqueue event loop (Story 4.1)
17. Alicorn `.app` launcher (Story 5.3)
18. p0f socket path (Story 1.5 addendum)

**Validation:** Performance benchmarks showing kqueue improvement. `.app` launcher user testing.

---

## 9. Success Criteria

### 9.1 Functional Requirements

- [x] `./configure && make && make install` succeeds on macOS 13+ ARM64
- [x] Homebrew formula created (`macos/unicornscan.rb`)
- [x] TCP SYN scan produces correct results on macOS
- [x] UDP scan produces correct results on macOS
- [x] ARP scan works with sudo privileges
- [ ] ICMP scan validation (not yet tested)
- [x] Payload modules load and function (except httpexp x86 shellcode)
- [x] Alicorn management script is cross-platform
- [x] GeoIP search paths include Homebrew locations
- [x] OS fingerprinting database includes macOS/iOS signatures
- [x] fantaip works on macOS (BPF contention fix applied)
- [x] fantaip + unicornscan coexist simultaneously

### 9.2 Non-Functional Requirements

- [x] No regression in Linux functionality (Linux build paths unchanged)
- [x] macOS build job in release workflow (`.github/workflows/release.yml`)
- [x] Scanner startup time acceptable
- [x] Clean process exit (no stale unilisten/unisend after scan)
- [ ] Zero high/critical security findings in macOS-specific code (not audited)

---

## 10. Appendix

### A. Platform API Mapping

| Linux API | macOS Equivalent | Used In |
|-----------|------------------|---------|
| `AF_PACKET` / `sockaddr_ll` | `AF_LINK` / `sockaddr_dl` | `intf.c` |
| `/proc/net/route` | `sysctl(CTL_NET, PF_ROUTE)` | `route.c` |
| `setcap cap_net_raw+ep` | ChmodBPF LaunchDaemon | Installer |
| `chroot()` | `sandbox_init()` | `arch.c` |
| `poll()` | `kqueue()` | `xpoll.c` |
| `SIOCETHTOOL` ioctl | N/A (not needed on macOS) | `pcaputil.c` |
| `ss -tuln` | `lsof -i :PORT` | Management script |
| `groupadd --system` | `dseditgroup -o create` | Installer |
| `usermod -aG` | `dseditgroup -o edit -a` | Installer |
| `systemd` | `launchd` / `launchctl` | Service management |

### B. Homebrew Path Reference

| Component | Homebrew Path (Apple Silicon) |
|-----------|-------------------------------|
| Prefix | `/opt/homebrew` |
| Binaries | `/opt/homebrew/bin` |
| Libraries | `/opt/homebrew/lib` |
| Headers | `/opt/homebrew/include` |
| Config | `/opt/homebrew/etc/unicornscan` |
| Var data | `/opt/homebrew/var/unicornscan` |
| Modules | `/opt/homebrew/lib/unicornscan/modules` |
| Libexec | `/opt/homebrew/libexec/unicornscan` |
| GeoIP DBs | `/opt/homebrew/share/GeoIP` |
| Man pages | `/opt/homebrew/share/man/man1` |

### C. DMG Install Path Reference

| Component | DMG Install Path |
|-----------|-----------------|
| App | `/Applications/Alicorn.app` |
| Binaries | `/usr/local/bin` |
| Libraries | `/usr/local/lib/unicornscan` |
| Config | `/usr/local/etc/unicornscan` |
| Modules | `/usr/local/lib/unicornscan/modules` |
| Libexec | `/usr/local/libexec/unicornscan` |
| Runtime data | `~/Library/Application Support/unicornscan` |
| LaunchDaemon | `/Library/LaunchDaemons/org.unicornscan.ChmodBPF.plist` |
| GeoIP DBs | `/usr/local/share/GeoIP` |

---

## 11. Implementation Completion Status

**As of v0.5.0 (2026-03-20)**

### Story-Level Status

| Story | Description | Status | Commit/Notes |
|-------|-------------|--------|-------------|
| 1.1 | configure.ac Darwin case | Done | `USE_SETRE`, `HAVE_KQUEUE`, `HAVE_SANDBOX_INIT` all defined |
| 1.2 | Homebrew dependency detection | Done | Dynamic `brew --prefix` detection in configure.ac |
| 1.3 | ARM64 compiler flags | Done | `-fPIC` added for aarch64/arm64 |
| 1.4 | Conditional libdnet patches | Done | `uname -s` guard in libs/Makefile.in |
| 1.5 | macOS setcap alternative | Done | ChmodBPF instructions on Darwin |
| 2.1 | AF_LINK interface enumeration | Done | `sockaddr_dl` / `LLADDR` in intf.c |
| 2.2 | BSD sysctl routing | Done | Full sysctl `NET_RT_DUMP` in route.c |
| 2.3 | pcap_setdirection guard | Done | `#ifdef PCAP_D_IN` with non-fatal fallback |
| 2.4 | Packet injection / BPF | Done | fantaip BPF reorder fix; libdnet `/dev/bpf0` limitation documented |
| 2.5 | ntalk.c htons fix | Done | `sin_family` no longer byte-swapped |
| 3.1 | ChmodBPF LaunchDaemon | Done | `macos/org.unicornscan.ChmodBPF.plist` + script |
| 3.2 | macOS sandbox profile | **Deferred** | `sandbox_init()` deprecated; API rejects custom profiles on macOS 10.15+ |
| 4.1 | kqueue event loop | Done | Full kqueue backend in xpoll.c with EV_EOF handling |
| 5.1 | Cross-platform Alicorn script | Done | OS detection + conditional paths |
| 5.2 | GeoIP update script | Done | macOS paths added |
| 5.3 | Alicorn .app launcher | Done | `macos/Alicorn.app` bundle |
| 6.1 | Homebrew formula | Done | `macos/unicornscan.rb` (243 lines) |
| 6.2 | DMG installer | Done | `macos/dmg/build-dmg.sh` (594 lines) |
| 6.3 | Release workflow macOS job | Done | `build-macos` job on `macos-14` runner |
| 7.1 | GeoIP Homebrew paths | Done | `/opt/homebrew/share/GeoIP` in all GeoIP modules |

### Summary

- **Total stories:** 20
- **Done:** 19 (95%)
- **Deferred:** 1 (Story 3.2 — macOS sandbox, API limitation)

### Known Issues

1. **libdnet `/dev/bpf0` hardcoding** — Homebrew libdnet 1.18.2 only tries `/dev/bpf0`. Mitigated by open ordering in fantaip. Long-term fix: patch libdnet or replace with native BPF wrapper.
2. **Sandbox not available** — `sandbox_init(SANDBOX_NAMED)` is deprecated and non-functional for custom profiles. Process isolation relies on non-root execution via ChmodBPF.
3. **pcap timeout** — macOS BPF rejects negative timeout values (`BIOCSRTIMEOUT`). All pcap_open_live calls must use non-negative timeout (fixed in fantaip, already correct in recv_packet.c).
