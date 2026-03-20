/**********************************************************************
 * Copyright (C) 2004-2006 (Jack Louis) <jack@rapturesecurity.org>    *
 *                                                                    *
 * This program is free software; you can redistribute it and/or      *
 * modify it under the terms of the GNU General Public License        *
 * as published by the Free Software Foundation; either               *
 * version 2 of the License, or (at your option) any later            *
 * version.                                                           *
 *                                                                    *
 * This program is distributed in the hope that it will be useful,    *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      *
 * GNU General Public License for more details.                       *
 *                                                                    *
 * You should have received a copy of the GNU General Public License  *
 * along with this program; if not, write to the Free Software        *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.          *
 **********************************************************************/

/*
 * eth_bpf_macos.c
 *
 * Replacement eth_open/eth_send/eth_get/eth_set/eth_close that iterates
 * /dev/bpf0 through /dev/bpf255 on macOS instead of hardcoding /dev/bpf0.
 *
 * Homebrew libdnet 1.18.2 hardcodes /dev/bpf0 in eth_open(), which causes
 * EBUSY failures when pcap, Wireshark, or unilisten already holds bpf0.
 * This file shadows libdnet's eth_open symbol (libunilib is linked before
 * -ldnet) so the linker embeds this implementation directly in the binary.
 *
 * Linux is not affected: the entire file is compiled away by #ifdef __APPLE__.
 * Linux uses PF_PACKET sockets and never calls eth_open at all via eth-linux.c
 * in libdnet.
 *
 * Reference implementation: libdnet 1.10 src/eth-bsd.c which iterated
 * /dev/bpf0 through /dev/bpf31.  This extends that range to /dev/bpf255
 * to match the macOS kernel's actual BPF device limit.
 */

#ifdef __APPLE__

#include <config.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Homebrew libdnet declares eth_t as:
 *   typedef struct eth_handle eth_t;
 * and keeps struct eth_handle private in eth-bsd.c.  Since callers treat
 * eth_t * as an opaque handle (they never access fields directly), we define
 * our own struct eth_handle here.  The layout matches Homebrew libdnet 1.18.2
 * exactly (fd + device[16]) so that any future code inspecting the struct
 * through a shared header will remain compatible.
 */
struct eth_handle {
	int	fd;
	char	device[16];
};

/*
 * eth_t typedef mirrors dnet/eth.h so this translation unit is self-contained.
 * Do NOT include dnet.h here: including it would pull in the dnet/eth.h
 * declaration of struct eth_handle as an incomplete type, which would conflict
 * with our complete definition above.
 */
typedef struct eth_handle eth_t;

/*
 * eth_addr_t mirrors dnet/eth.h.  Defined here for eth_get/eth_set.
 */
#define ETH_ADDR_LEN	6
#define ETH_ADDR_BITS	48

typedef struct eth_addr {
	unsigned char	data[ETH_ADDR_LEN];
} eth_addr_t;

/* maximum BPF device index to probe */
#define BPF_MAX_DEV_IDX		255

/*
 * eth_open -- open an ethernet handle for the named interface.
 *
 * Iterates /dev/bpf0 through /dev/bpf255 until a device opens without EBUSY.
 * Binds the BPF file descriptor to @device via BIOCSETIF, then enables
 * header-complete mode via BIOCSHDRCMPLT so the caller supplies the full
 * Ethernet frame including source/destination MAC addresses.
 *
 * Returns a pointer to a heap-allocated eth_t on success.
 * Returns NULL on failure (all BPF devices busy, BIOCSETIF error, or
 * BIOCSHDRCMPLT error).
 */
eth_t *
eth_open(const char *device)
{
	struct ifreq ifr;
	char bpf_path[32];
	eth_t *e;
	int bpf_idx;
	int hdr_cmplt;

	e = calloc(1, sizeof(*e));
	if (e == NULL) {
		return (NULL);
	}

	e->fd = -1;

	/*
	 * Iterate /dev/bpf0 through /dev/bpf255.  Skip EBUSY entries.
	 * Any other open(2) error terminates the search immediately because
	 * it indicates a permanent condition (e.g. permission denied).
	 */
	for (bpf_idx = 0; bpf_idx <= BPF_MAX_DEV_IDX; bpf_idx++) {
		snprintf(bpf_path, sizeof(bpf_path), "/dev/bpf%d", bpf_idx);
		e->fd = open(bpf_path, O_WRONLY);
		if (e->fd >= 0) {
			break;
		}
		if (errno != EBUSY) {
			/* permanent error: permission denied, no such file, etc */
			break;
		}
		/* errno == EBUSY: this device is held by another process */
	}

	if (e->fd < 0) {
		free(e);
		return (NULL);
	}

	/*
	 * Bind the BPF descriptor to the requested network interface.
	 * BIOCSETIF fails if the interface does not exist or if the fd
	 * cannot be attached (e.g. the device does not support BPF).
	 */
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));

	if (ioctl(e->fd, BIOCSETIF, (char *)&ifr) < 0) {
		close(e->fd);
		free(e);
		return (NULL);
	}

	/*
	 * Enable header-complete mode: the caller constructs the full
	 * Ethernet frame, including source and destination MAC addresses.
	 * Without this flag the kernel overwrites the source MAC with the
	 * interface's hardware address on every write(2) call, breaking ARP
	 * spoofing and raw frame injection.
	 */
#ifdef BIOCSHDRCMPLT
	hdr_cmplt = 1;
	if (ioctl(e->fd, BIOCSHDRCMPLT, &hdr_cmplt) < 0) {
		close(e->fd);
		free(e);
		return (NULL);
	}
#endif

	strlcpy(e->device, device, sizeof(e->device));

	return (e);
}

/*
 * eth_send -- write a raw Ethernet frame.
 *
 * Writes @len bytes from @buf to the BPF file descriptor.  The caller is
 * responsible for supplying a complete Ethernet frame.
 *
 * Returns the number of bytes written, or -1 on error (errno set by write).
 */
ssize_t
eth_send(eth_t *e, const void *buf, size_t len)
{
	return (write(e->fd, buf, len));
}

/*
 * eth_close -- close an ethernet handle and release associated resources.
 *
 * Always returns NULL so the caller can write:
 *   handle = eth_close(handle);
 * and be left with a NULL pointer.
 */
eth_t *
eth_close(eth_t *e)
{
	if (e != NULL) {
		if (e->fd >= 0) {
			close(e->fd);
		}
		free(e);
	}
	return (NULL);
}

/*
 * eth_get -- retrieve the hardware (MAC) address of the bound interface.
 *
 * Uses sysctl(CTL_NET, AF_ROUTE, NET_RT_IFLIST) to walk the interface list
 * and locate the link-layer address for e->device.
 *
 * Returns 0 on success, -1 on error (errno set to ESRCH if interface not
 * found in the routing table, or the sysctl error otherwise).
 */
int
eth_get(eth_t *e, eth_addr_t *ea)
{
	struct if_msghdr *ifm;
	struct sockaddr_dl *sdl;
	unsigned char *p;
	unsigned char *buf;
	size_t buflen;
	int mib[6];

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_LINK;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &buflen, NULL, 0) < 0) {
		return (-1);
	}

	buf = malloc(buflen);
	if (buf == NULL) {
		return (-1);
	}

	if (sysctl(mib, 6, buf, &buflen, NULL, 0) < 0) {
		free(buf);
		return (-1);
	}

	for (p = buf; p < buf + buflen; p += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)p;
		sdl = (struct sockaddr_dl *)(ifm + 1);

		if (ifm->ifm_type != RTM_IFINFO ||
		    (ifm->ifm_addrs & RTA_IFP) == 0) {
			continue;
		}
		if (sdl->sdl_family != AF_LINK || sdl->sdl_nlen == 0) {
			continue;
		}
		if (memcmp(sdl->sdl_data, e->device, sdl->sdl_nlen) != 0) {
			continue;
		}
		/* found the interface: copy the link-layer address */
		if (sdl->sdl_alen == ETH_ADDR_LEN) {
			memcpy(ea->data,
			       sdl->sdl_data + sdl->sdl_nlen,
			       ETH_ADDR_LEN);
			free(buf);
			return (0);
		}
	}

	free(buf);
	errno = ESRCH;
	return (-1);
}

/*
 * eth_set -- set the hardware (MAC) address of the bound interface.
 *
 * Uses SIOCSIFLLADDR ioctl to program a new link-layer address.
 * This ioctl is macOS-specific; on systems without it the function
 * returns -1 with errno ENOSYS (handled by the #else branch below).
 *
 * Returns 0 on success, -1 on error (errno set by ioctl).
 */
#if defined(SIOCSIFLLADDR)
int
eth_set(eth_t *e, const eth_addr_t *ea)
{
	struct ifreq ifr;
	struct sockaddr sdl_sa;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, e->device, sizeof(ifr.ifr_name));

	memset(&sdl_sa, 0, sizeof(sdl_sa));
	sdl_sa.sa_family = AF_LINK;
	sdl_sa.sa_len = ETH_ADDR_LEN;
	memcpy(sdl_sa.sa_data, ea->data, ETH_ADDR_LEN);
	ifr.ifr_addr = sdl_sa;

	return (ioctl(e->fd, SIOCSIFLLADDR, &ifr));
}
#else
int
eth_set(eth_t *e, const eth_addr_t *ea)
{
	(void)e;
	(void)ea;
	errno = ENOSYS;
	return (-1);
}
#endif /* SIOCSIFLLADDR */

#endif /* __APPLE__ */
