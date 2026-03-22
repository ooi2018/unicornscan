# Unicornscan Privilege Model — macOS

**Version:** 0.4.52
**Platform:** macOS (Darwin)

---

macOS enforces two separate privilege gates for raw packet I/O that do not
exist on Linux. Understanding which gate each scan mode passes through is
necessary for configuring a system correctly and for diagnosing permission
errors.

## 1. The Two Privilege Gates

### 1.1 `ip_open()` — Raw IP socket (SOCK_RAW)

`ip_open()` creates an `AF_INET / SOCK_RAW` socket via libdnet. macOS requires
the calling process to be running as root (effective UID 0) to create a raw
socket. There is no alternative mechanism; the kernel returns `EPERM` to any
non-root caller regardless of group membership or ACLs.

**Minimum privilege: root (sudo or setuid)**

Source: `src/scan_progs/send_packet.c:1259-1268`

```c
case SOCK_IP:
    if (sl.s_u.ipsock == NULL) {
        sl.s_u.ipsock=ip_open();
        if (sl.s_u.ipsock == NULL) {
#ifdef __APPLE__
            ERR("raw socket access requires root on macOS, try: sudo unicornscan ...");
#endif
            terminate("dnet ip_open fails");
        }
    }
```

### 1.2 `eth_open()` — BPF device (link-layer)

`eth_open()` opens a `/dev/bpf*` device via libdnet to send raw Ethernet
frames. On macOS these device nodes are created on demand by the kernel and
are owned by root with mode 0600 by default. Opening them therefore also
requires root unless their permissions have been changed.

The ChmodBPF mechanism (modelled after Wireshark's approach) changes the group
ownership of every `/dev/bpf*` device to `unicornscan` and sets mode 0660.
Once that is in place, any user who is a member of the `unicornscan` group
can open BPF devices without root.

**Minimum privilege with ChmodBPF installed: membership in the `unicornscan` group**
**Minimum privilege without ChmodBPF: root**

Source: `src/scan_progs/send_packet.c:1271-1281`

```c
case SOCK_LL:
    if (sl.s_u.llsock == NULL) {
        sl.s_u.llsock=eth_open(s->interface_str);
        if (sl.s_u.llsock == NULL) {
#ifdef __APPLE__
            ERR("eth_open `%s' fails: all /dev/bpf* devices may be busy",
                s->interface_str);
            ERR("check: lsof /dev/bpf* | grep -v unicornscan");
#endif
            terminate("dnet eth_open `%s' fails", s->interface_str);
        }
    }
```

---

## 2. Privilege Matrix

The `open_link()` function in `send_packet.c` is the single dispatch point
that selects `SOCK_IP` or `SOCK_LL` for every incoming workunit. The dispatch
is driven by the workunit magic value set by the parser.

| Flag | Mode name | Socket path | Sender function | Privilege without ChmodBPF | Privilege with ChmodBPF |
|------|-----------|-------------|-----------------|----------------------------|-------------------------|
| `-mT` (default) | TCP SYN scan | `SOCK_IP` → `ip_open()` | `ip_send()` | **root** | **root** |
| `-mT` + flag modifiers | TCP custom flags (ACK, FIN, etc.) | `SOCK_IP` → `ip_open()` | `ip_send()` | **root** | **root** |
| `-msf` | TCP connect scan | `SOCK_IP` → `ip_open()` | `ip_send()` via PRI_4SEND_MAGIC | **root** | **root** |
| `-mtr` | TCP traceroute | `SOCK_IP` → `ip_open()` | `ip_send()` | **root** | **root** |
| `-mU` | UDP scan | `SOCK_IP` → `ip_open()` | `ip_send()` | **root** | **root** |
| `-mA` | ARP scan | `SOCK_LL` → `eth_open()` | `eth_send()` | **root** | **unicornscan group** |
| `-mA+T` | ARP + TCP (compound) | ARP phase: `SOCK_LL`; TCP phase: `SOCK_IP` | `eth_send()` then `ip_send()` | **root** | **root** (TCP phase requires root) |
| `-mA+U` | ARP + UDP (compound) | ARP phase: `SOCK_LL`; UDP phase: `SOCK_IP` | `eth_send()` then `ip_send()` | **root** | **root** (UDP phase requires root) |
| `-mA+sf` | ARP + TCP connect (compound) | ARP phase: `SOCK_LL`; TCP phase: `SOCK_IP` | `eth_send()` then `ip_send()` | **root** | **root** (TCP phase requires root) |

Source evidence for each row:

- `TCP_SEND_MAGIC` → `open_link(SOCK_IP, ...)` at line 529
- `TCPTRACE_SEND_MAGIC` → `open_link(SOCK_IP, ...)` at line 537
- `UDP_SEND_MAGIC` → `open_link(SOCK_IP, ...)` at line 545
- `ARP_SEND_MAGIC` → `open_link(SOCK_LL, ...)` at line 553
- `PRI_4SEND_MAGIC` (connect scan follow-up packets) → `open_link(SOCK_IP, ...)` at line 459

`MODE_ICMPSCAN` (bit 8) and `MODE_IPSCAN` (bit 16) are defined in
`scan_export.h` and referenced in `workunits.c` and `scanopts.c` but have no
corresponding workunit magic handler in `send_packet.c`. Any workunit carrying
`ICMP_SEND_MAGIC` or `IP_SEND_MAGIC` falls through to the `ERR("unknown
workunit type")` branch and is discarded. These modes are not operational
and are therefore not listed above.

### Summary

The only scan mode that benefits from ChmodBPF (and therefore does not
unconditionally require root) is ARP-only (`-mA`). All other modes — TCP, UDP,
TCP traceroute, TCP connect — send through `ip_open()` which requires root on
macOS regardless of group membership.

---

## 3. ChmodBPF Setup

ChmodBPF is a small shell script that creates the `unicornscan` group,
changes the group ownership of every `/dev/bpf*` device to that group, and
sets the device mode to 0660. A LaunchDaemon (`org.unicornscan.ChmodBPF`)
re-applies these permissions at every system startup because macOS resets BPF
device ownership on reboot.

### 3.1 Automated installation (recommended)

The installer copies ChmodBPF to `/usr/local/bin/`, installs the LaunchDaemon
plist to `/Library/LaunchDaemons/`, loads the daemon, and adds the invoking
user to the `unicornscan` group.

```bash
sudo macos/install-chmodbpf.sh
```

After installation you must log out and log back in (or reboot) for the group
membership change to take effect in your shell session.

### 3.2 Verify the installation

```bash
# Confirm the daemon is loaded
launchctl list org.unicornscan.ChmodBPF

# Confirm the BPF devices have the correct group and permissions
ls -l /dev/bpf* | head

# Expected output format:
# crw-rw---- 1 root unicornscan  ... /dev/bpf0
# crw-rw---- 1 root unicornscan  ... /dev/bpf1

# Confirm your user is in the group
dseditgroup -o checkmember -m "$(whoami)" unicornscan
```

### 3.3 Add additional users

Each user who needs to run ARP scans without root must be added to the group:

```bash
sudo dseditgroup -o edit -a USERNAME -t user unicornscan
```

The user must then log out and back in for the membership to activate.

### 3.4 Manual invocation (without LaunchDaemon)

Run ChmodBPF directly when you need to restore permissions without a reboot:

```bash
sudo /usr/local/bin/ChmodBPF
```

### 3.5 Uninstall

```bash
sudo macos/uninstall-chmodbpf.sh
```

This unloads the daemon and removes `/usr/local/bin/ChmodBPF` and the plist.
BPF device permissions revert to system defaults at the next reboot. The script
offers to delete the `unicornscan` group interactively.

---

## 4. Troubleshooting

### 4.1 `ip_open fails` / `raw socket access requires root on macOS`

**Symptom:** unicornscan exits immediately with:

```
raw socket access requires root on macOS, try: sudo unicornscan ...
dnet ip_open fails
```

**Cause:** The process is not running as root and attempted to open a
`SOCK_RAW` socket. This happens for any TCP, UDP, TCP traceroute, or TCP
connect scan.

**Fix:** Run unicornscan with `sudo`:

```bash
sudo unicornscan [options] target
```

ChmodBPF group membership does not help here. Raw IP sockets require root on
macOS unconditionally.

---

### 4.2 `eth_open fails` / BPF device busy or inaccessible

**Symptom:** unicornscan exits with:

```
eth_open 'en0' fails: all /dev/bpf* devices may be busy
check: lsof /dev/bpf* | grep -v unicornscan
dnet eth_open 'en0' fails
```

**Cause A — Not in the `unicornscan` group:** ChmodBPF is not installed, or
your user session predates your group membership change (you have not logged
out and back in since being added to the group).

**Diagnosis:**

```bash
# Check whether ChmodBPF has run and set permissions
ls -l /dev/bpf0

# If ownership is NOT unicornscan:
# crw------- 1 root wheel ... /dev/bpf0   <- ChmodBPF has not run
# crw-rw---- 1 root unicornscan ... /dev/bpf0  <- ChmodBPF has run

# Check group membership in the current session
id | grep unicornscan
```

**Fix:**

1. Install ChmodBPF if not already done: `sudo macos/install-chmodbpf.sh`
2. Log out and log back in (group membership requires a new login session)
3. Or run as root: `sudo unicornscan -mA [target]`

**Cause B — All BPF devices are held by another process:** macOS limits the
number of simultaneous BPF readers. Wireshark, tcpdump, Little Snitch, or any
other packet capture tool running concurrently may exhaust the available
devices.

**Diagnosis:**

```bash
lsof /dev/bpf* | grep -v unicornscan
```

**Fix:** Stop the conflicting capture process, or wait for it to release the
device.

---

### 4.3 `pcap permission denied`

**Symptom:** The listener process (which uses libpcap to capture responses)
fails with a permission denied error on `/dev/bpf*`.

**Cause:** Same as 4.2 — the user does not have read access to BPF devices.
The sender and the listener both require BPF access on macOS; for ARP scans
both paths use the link layer.

**Fix:** Same as 4.2 — install ChmodBPF and rejoin the session, or run with
`sudo`.

**Additional check:**

```bash
# Confirm pcap can open the interface as your user
python3 -c "import subprocess; subprocess.run(['tcpdump','-i','en0','-c','1','-q'])"
# If this also fails with permission denied, ChmodBPF is not in effect.
```

---

### 4.4 Group membership appears not to take effect

macOS caches group membership in the login session. Adding a user to a group
via `dseditgroup` does not propagate to running processes.

**Fix:** Log out completely and log back in, or reboot. Launching a new
Terminal window is not sufficient; the session token must be refreshed.

**Verification after re-login:**

```bash
id | grep unicornscan
# Should show: ...,unicornscan,...

# Confirm BPF access without sudo
cat /dev/bpf0 >/dev/null 2>&1 && echo "BPF access OK" || echo "BPF access denied"
```

---

### 4.5 Daemon log

The ChmodBPF LaunchDaemon writes to:

```
/var/log/unicornscan-chmodbpf.log
```

Inspect this file if device permissions are not being applied at boot:

```bash
cat /var/log/unicornscan-chmodbpf.log
```

---

## 5. Quick-Reference Summary

| Situation | Minimum requirement |
|-----------|---------------------|
| TCP SYN scan (`-mT`) | `sudo` |
| TCP custom flags (`-mTSF`, `-mTsA`, etc.) | `sudo` |
| TCP connect scan (`-msf`) | `sudo` |
| TCP traceroute (`-mtr`) | `sudo` |
| UDP scan (`-mU`) | `sudo` |
| ARP scan (`-mA`) without ChmodBPF | `sudo` |
| ARP scan (`-mA`) with ChmodBPF installed | `unicornscan` group membership |
| Compound scan with any non-ARP phase | `sudo` |
