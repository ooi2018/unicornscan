#!/bin/sh
#
# uninstall-chmodbpf.sh — Remove the unicornscan ChmodBPF LaunchDaemon
#
# Unloads the daemon, removes installed files, and optionally deletes the
# unicornscan group.  Does NOT modify /dev/bpf* permissions (they will
# revert to defaults at next reboot).
#
# Must be run as root (or via sudo).
#

set -e

SCRIPT_NAME="$(basename "$0")"

BPF_GROUP="unicornscan"
CHMODBPF_DST="/usr/local/bin/ChmodBPF"
PLIST_DST="/Library/LaunchDaemons/org.unicornscan.ChmodBPF.plist"
LOG_FILE="/var/log/unicornscan-chmodbpf.log"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

die() {
    echo "${SCRIPT_NAME}: ERROR: $1" >&2
    exit 1
}

info() {
    echo "${SCRIPT_NAME}: $1"
}

# ---------------------------------------------------------------------------
# Pre-flight checks
# ---------------------------------------------------------------------------

if [ "$(id -u)" -ne 0 ]; then
    die "This script must be run as root.  Use: sudo $0"
fi

if [ "$(uname -s)" != "Darwin" ]; then
    die "This script is intended for macOS (Darwin) only."
fi

# ---------------------------------------------------------------------------
# Unload the LaunchDaemon if it is loaded
# ---------------------------------------------------------------------------

if launchctl list org.unicornscan.ChmodBPF >/dev/null 2>&1; then
    info "Unloading LaunchDaemon..."
    launchctl unload "${PLIST_DST}" 2>/dev/null || true
    info "LaunchDaemon unloaded."
else
    info "LaunchDaemon is not currently loaded."
fi

# ---------------------------------------------------------------------------
# Remove installed files
# ---------------------------------------------------------------------------

if [ -f "${PLIST_DST}" ]; then
    info "Removing ${PLIST_DST} ..."
    rm -f "${PLIST_DST}"
else
    info "${PLIST_DST} not found (already removed)."
fi

if [ -f "${CHMODBPF_DST}" ]; then
    info "Removing ${CHMODBPF_DST} ..."
    rm -f "${CHMODBPF_DST}"
else
    info "${CHMODBPF_DST} not found (already removed)."
fi

if [ -f "${LOG_FILE}" ]; then
    info "Removing ${LOG_FILE} ..."
    rm -f "${LOG_FILE}"
else
    info "${LOG_FILE} not found (already removed)."
fi

# ---------------------------------------------------------------------------
# Optionally delete the group
# ---------------------------------------------------------------------------

if dseditgroup -o read "${BPF_GROUP}" >/dev/null 2>&1; then
    echo ""
    echo "The '${BPF_GROUP}' group still exists in the directory service."
    echo ""
    # In non-interactive (piped) contexts, default to keeping the group.
    if [ -t 0 ]; then
        printf "Delete the '%s' group? [y/N] " "${BPF_GROUP}"
        read -r answer
        case "${answer}" in
            [Yy]|[Yy][Ee][Ss])
                info "Deleting group '${BPF_GROUP}'..."
                if dseditgroup -o delete "${BPF_GROUP}"; then
                    info "Group '${BPF_GROUP}' deleted."
                else
                    info "WARNING: Failed to delete group '${BPF_GROUP}'. You can remove it manually:"
                    info "  sudo dseditgroup -o delete ${BPF_GROUP}"
                fi
                ;;
            *)
                info "Keeping group '${BPF_GROUP}'."
                ;;
        esac
    else
        info "Running non-interactively; keeping group '${BPF_GROUP}'."
        info "To delete it manually:  sudo dseditgroup -o delete ${BPF_GROUP}"
    fi
else
    info "Group '${BPF_GROUP}' does not exist (already removed)."
fi

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

echo ""
echo "=========================================================="
echo "  Uninstallation complete."
echo ""
echo "  BPF device permissions will revert to system defaults"
echo "  at the next reboot."
echo ""
echo "  If you kept the '${BPF_GROUP}' group, existing user"
echo "  memberships are unchanged.  Remove them with:"
echo "    sudo dseditgroup -o edit -d USERNAME -t user ${BPF_GROUP}"
echo "=========================================================="
echo ""

exit 0
