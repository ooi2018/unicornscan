/**********************************************************************
 * Copyright (C) 2005-2006 (Jack Louis) <jack@rapturesecurity.org>    *
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
#include <config.h>
#include <settings.h>
#include <net/route.h>

#include <unilib/xmalloc.h>
#include <unilib/output.h>
#include <unilib/cidr.h>

#include <patricia.h>

static void get_netroutes(void);
static int masktocidr(uint32_t );

static int need_netroutes=1;
static patricia_tree_t *rt=NULL;
static patricia_node_t *node=NULL;

typedef union route_info_t {
	struct info_s {
		char *intf;
		uint16_t metric;
		uint16_t flags;
		struct sockaddr_storage gw;
	} *info_s;
	void *p;
} route_info_t;

int getroutes(char **intf, struct sockaddr *tgt, struct sockaddr *tgtmask, struct sockaddr **gw) {
	static char lookup[128];
	route_info_t ri_u;
	union sock_u ts_u, gws_u;
	static struct sockaddr_storage gw_s;
	const char *rstr=NULL;
	unsigned int rmask=0;

	assert(intf != NULL && tgt != NULL && tgtmask != NULL && gw != NULL);

	ts_u.s=tgt;
	*gw=NULL;

	rstr=cidr_saddrstr(tgt);
	if (rstr == NULL) {
		return -1;
	}

	rmask=cidr_getmask(tgtmask);

	snprintf(lookup, sizeof(lookup) -1, "%s/%u", rstr, rmask);

	DBG(M_RTE, "looking up route for `%s'", lookup);

	if (need_netroutes) {
		get_netroutes();
	}

	node=try_search_best(rt, lookup);
	if (node == NULL) {
		ERR("no route to host for `%s'", lookup);
		*intf=NULL;
		*gw=NULL;
		return -EHOSTUNREACH;
	}
	ri_u.p=node->data;
	assert(node->data != NULL);

	DBG(M_RTE, "found interface `%s' for network `%s'", ri_u.info_s->intf, lookup);

	*intf=ri_u.info_s->intf;
	if (ri_u.info_s->gw.ss_family != 0) {
		memcpy(&gw_s, &ri_u.info_s->gw, sizeof(struct sockaddr_storage));
		gws_u.ss=&gw_s;
		*gw=gws_u.s;
	}
	else {
		*gw=NULL;
	}

	return 1;
}

static int masktocidr(uint32_t mask) {
	int j=0, cidr=0;

	/* endian */
	for (j=0; j < 32; j++) {
		if ((mask & 0x80000000) == 0x80000000) {
			cidr++;
		}
		mask <<= 1;
	}

	return cidr;
}

#ifdef HAVE_PROC_NET_ROUTE

static void get_netroutes(void) {
	FILE *pnr=NULL;
	char lbuf[1024], intf[32];
	uint32_t dest, gw, refcnt, use, mask, irtt;
	uint16_t metric, flags, window, mtu;
	char destnet[128], gwstr[128], addstr[192];
	int lineno=0;

	pnr=fopen("/proc/net/route", "r");
	if (pnr == NULL) {
		ERR("cant open /proc/net/route: `%s'", strerror(errno));
		exit(1);
	}

	rt=New_Patricia(128);

	/*
	 * Iface   Destination     Gateway         Flags   RefCnt  Use     Metric  Mask            MTU     Window  IRTT
	 * eth1    0045A8C0        00000000        0001    0       0       0       00FFFFFF        0       0       0
	 */

	for (lineno=0; fgets(lbuf, sizeof(lbuf) -1, pnr) != NULL; lineno++) {
		if (lineno == 0) {
			continue;
		}
#if 0
#define RTF_UP          0x0001          /* route usable                 */
#define RTF_GATEWAY     0x0002          /* destination is a gateway     */
#define RTF_HOST        0x0004          /* host entry (net otherwise)   */
#define RTF_REINSTATE   0x0008          /* reinstate route after tmout  */
#define RTF_DYNAMIC     0x0010          /* created dyn. (by redirect)   */
#define RTF_MODIFIED    0x0020          /* modified dyn. (by redirect)  */
#define RTF_MTU         0x0040          /* specific MTU for this route  */
#define RTF_MSS         RTF_MTU         /* Compatibility :-(            */
#define RTF_WINDOW      0x0080          /* per route window clamping    */
#define RTF_IRTT        0x0100          /* Initial round trip time      */
#define RTF_REJECT      0x0200          /* Reject route                 */
#endif
		/*                 in  de gw fl  ref us me ma mt  wi  ir	*/
		if (sscanf(lbuf, "%31s %x %x %hx %u %u %hu %x %hu %hu %u", intf, &dest, &gw, &flags, &refcnt, &use, &metric, &mask, &mtu, &window, &irtt) == 11) {
			int mycidr=0;
			struct in_addr ia;

			ia.s_addr=dest;
			inet_ntop(AF_INET, &ia, destnet, sizeof(destnet));
			mycidr=masktocidr(mask);
			ia.s_addr=gw;
			inet_ntop(AF_INET, &ia, gwstr, sizeof(gwstr));

			if (flags & RTF_UP && mycidr > -1) {
				union sock_u s_u;
				route_info_t ri_u;

				ri_u.p=xmalloc(sizeof(*ri_u.info_s));
				memset(ri_u.p, 0, sizeof(*ri_u.info_s));

				ri_u.info_s->intf=xstrdup(intf);
				ri_u.info_s->metric=metric; /* could only be 0xff anyhow */
				ri_u.info_s->flags=flags;
				if ((flags & RTF_GATEWAY) == RTF_GATEWAY) {
					s_u.ss=&ri_u.info_s->gw;
					s_u.sin->sin_addr.s_addr=gw;
					s_u.sin->sin_family=AF_INET;
				}

				snprintf(addstr, sizeof(addstr), "%s/%d", destnet, mycidr);
				DBG(M_RTE, "net %s via %s metric %u", addstr, (flags & RTF_GATEWAY) == 0 ? intf : gwstr, metric);
				node=make_and_lookup(rt, addstr);
				if (node == NULL) {
					exit(1);
				}
				node->data=ri_u.p;

			}
		}
		else {
			ERR("can not parse `%s'", lbuf);
		}
	}

	fclose(pnr);
	need_netroutes=0;

	return;
}

#elif defined(HAVE_SYS_SYSCTL_H) /* BSD/macOS: use sysctl routing table */

#include <sys/sysctl.h>
#include <net/if_dl.h>

/*
 * ROUNDUP/NEXTSA macros for walking variable-length sockaddr structures
 * in BSD routing messages. Based on FreeBSD /usr/src/sbin/route.c convention.
 */
#define RT_ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

#ifdef HAVE_STRUCT_SOCKADDR_LEN
#define RT_NEXTSA(s) \
	((struct sockaddr *)((u_char *)(s) + RT_ROUNDUP((s)->sa_len)))
#else
#define RT_NEXTSA(s) \
	((struct sockaddr *)((u_char *)(s) + RT_ROUNDUP(sizeof(*(s)))))
#endif

/*
 * Extract sockaddrs from a routing message based on the rtm_addrs bitmask.
 * BSD routing messages pack variable-length sockaddrs consecutively after
 * the rt_msghdr, with rtm_addrs indicating which addresses are present
 * (RTA_DST, RTA_GATEWAY, RTA_NETMASK, etc. in order of bit position).
 */
static void rt_extract_addrs(struct rt_msghdr *rtm, struct sockaddr **dst,
		struct sockaddr **gw, struct sockaddr **mask, struct sockaddr **ifp) {
	struct sockaddr *sa=NULL;
	int i=0;

	*dst=NULL;
	*gw=NULL;
	*mask=NULL;
	*ifp=NULL;

	sa=(struct sockaddr *)((char *)rtm + sizeof(*rtm));

	for (i=0; i < RTAX_MAX; i++) {
		if (rtm->rtm_addrs & (1 << i)) {
			switch (i) {
				case RTAX_DST:
					*dst=sa;
					break;
				case RTAX_GATEWAY:
					*gw=sa;
					break;
				case RTAX_NETMASK:
					*mask=sa;
					break;
				case RTAX_IFP:
					*ifp=sa;
					break;
				default:
					break;
			}
			sa=RT_NEXTSA(sa);
		}
	}

	return;
}

/*
 * Convert a BSD routing netmask sockaddr to CIDR prefix length.
 * BSD encodes netmasks with sa_len set to the minimum length needed
 * to represent the mask (trailing zero bytes are omitted), or sa_len=0
 * for the default route (0.0.0.0/0).
 */
static int rt_mask_to_cidr(struct sockaddr *mask) {
	int cidr=0;

	if (mask == NULL) {
		return 0;
	}

#ifdef HAVE_STRUCT_SOCKADDR_LEN
	if (mask->sa_len == 0) {
		return 0; /* default route */
	}
#endif

	if (mask->sa_family == AF_INET || mask->sa_family == 0) {
		/*
		 * BSD netmask sockaddrs may have sa_family=0 and a truncated
		 * length. We extract whatever bytes are present and treat
		 * missing trailing bytes as zero.
		 */
		uint8_t *p=NULL;
		int len=0;
		int j=0;

#ifdef HAVE_STRUCT_SOCKADDR_LEN
		/* sa_len includes the header (sa_len + sa_family = 2 bytes on BSD) */
		len=mask->sa_len - offsetof(struct sockaddr_in, sin_addr);
		if (len < 0) len=0;
		if (len > 4) len=4;
#else
		len=4;
#endif
		p=(uint8_t *)&((struct sockaddr_in *)mask)->sin_addr;
		for (j=0; j < len; j++) {
			uint8_t byte=p[j];
			while (byte & 0x80) {
				cidr++;
				byte <<= 1;
			}
			if (byte != 0) break; /* non-contiguous mask */
		}
	}

	return cidr;
}

/*
 * get_netroutes: populate patricia trie from BSD sysctl routing table.
 *
 * Uses sysctl({CTL_NET, PF_ROUTE, 0, 0, NET_RT_DUMP, 0}) to retrieve
 * the kernel routing table, then parses rt_msghdr structures to extract
 * destination, gateway, netmask, and interface for each route entry.
 */
static void get_netroutes(void) {
	union {
		void *p;
		char *step;
		struct rt_msghdr *rtm;
	} r_u;
	int mib[6];
	size_t alen=0;
	void *end=NULL, *start=NULL;
	struct sockaddr *sa_dst=NULL, *sa_gw=NULL, *sa_mask=NULL, *sa_ifp=NULL;

	mib[0]=CTL_NET;
	mib[1]=PF_ROUTE;
	mib[2]=0;		/* protocol */
	mib[3]=AF_INET;	/* IPv4 only */
	mib[4]=NET_RT_DUMP;
	mib[5]=0;		/* flags */

	if (sysctl(mib, 6, NULL, &alen, NULL, 0) < 0) {
		ERR("sysctl route table size estimate: %s", strerror(errno));
		return;
	}

	if (alen == 0) {
		ERR("sysctl returned empty routing table");
		return;
	}

	start=xmalloc(alen);
	end=(char *)start + alen;
	r_u.p=start;

	if (sysctl(mib, 6, r_u.p, &alen, NULL, 0) < 0) {
		ERR("sysctl route table read: %s", strerror(errno));
		xfree(start);
		return;
	}

	rt=New_Patricia(128);

	for (; r_u.p < end && r_u.rtm->rtm_msglen > 0; r_u.step += r_u.rtm->rtm_msglen) {
		char destnet[128], gwstr[128], addstr[192];
		char ifname[IF_NAMESIZE];
		int mycidr=0;
		struct in_addr ia;

		if (r_u.rtm->rtm_version != RTM_VERSION) {
			continue;
		}

		/* only process route entries that are UP */
		if ((r_u.rtm->rtm_flags & RTF_UP) == 0) {
			continue;
		}

		/* skip host routes from ARP cache (wascloned/llinfo entries) */
		if ((r_u.rtm->rtm_flags & RTF_LLINFO) != 0) {
			continue;
		}

		rt_extract_addrs(r_u.rtm, &sa_dst, &sa_gw, &sa_mask, &sa_ifp);

		if (sa_dst == NULL) {
			continue;
		}

		/* only handle IPv4 destinations for now */
		if (sa_dst->sa_family != AF_INET) {
			continue;
		}

		mycidr=rt_mask_to_cidr(sa_mask);

		ia=((struct sockaddr_in *)sa_dst)->sin_addr;
		inet_ntop(AF_INET, &ia, destnet, sizeof(destnet));

		/*
		 * Determine interface name: prefer sockaddr_dl from routing
		 * message, fall back to if_indextoname using rtm_index.
		 */
		memset(ifname, 0, sizeof(ifname));
#ifdef AF_LINK
		if (sa_ifp != NULL && sa_ifp->sa_family == AF_LINK) {
			struct sockaddr_dl *sdl=(struct sockaddr_dl *)sa_ifp;
			if (sdl->sdl_nlen > 0 && sdl->sdl_nlen < sizeof(ifname)) {
				memcpy(ifname, sdl->sdl_data, sdl->sdl_nlen);
				ifname[sdl->sdl_nlen]='\0';
			}
		}
#endif
		if (ifname[0] == '\0') {
			if (if_indextoname(r_u.rtm->rtm_index, ifname) == NULL) {
				snprintf(ifname, sizeof(ifname), "idx%d", r_u.rtm->rtm_index);
			}
		}

		/* build the route_info entry and insert into patricia trie */
		{
			union sock_u s_u;
			route_info_t ri_u;

			ri_u.p=xmalloc(sizeof(*ri_u.info_s));
			memset(ri_u.p, 0, sizeof(*ri_u.info_s));

			ri_u.info_s->intf=xstrdup(ifname);
			ri_u.info_s->metric=0; /* BSD does not expose metric in rt_msghdr */
			ri_u.info_s->flags=(uint16_t)r_u.rtm->rtm_flags;

			gwstr[0]='\0';
			if ((r_u.rtm->rtm_flags & RTF_GATEWAY) == RTF_GATEWAY && sa_gw != NULL && sa_gw->sa_family == AF_INET) {
				s_u.ss=&ri_u.info_s->gw;
				s_u.sin->sin_addr=((struct sockaddr_in *)sa_gw)->sin_addr;
				s_u.sin->sin_family=AF_INET;
				inet_ntop(AF_INET, &s_u.sin->sin_addr, gwstr, sizeof(gwstr));
			}

			snprintf(addstr, sizeof(addstr), "%s/%d", destnet, mycidr);
			DBG(M_RTE, "net %s via %s intf %s flags 0x%x", addstr,
				gwstr[0] != '\0' ? gwstr : "direct", ifname,
				r_u.rtm->rtm_flags);
			node=make_and_lookup(rt, addstr);
			if (node == NULL) {
				ERR("patricia insert failed for %s", addstr);
				xfree(ri_u.info_s->intf);
				xfree(ri_u.p);
				continue;
			}
			node->data=ri_u.p;
		}
	}

	xfree(start);
	need_netroutes=0;

	return;
}

#undef RT_ROUNDUP
#undef RT_NEXTSA

#else /* fallback: use libdnet/libdumbnet */

#ifdef HAVE_DUMBNET_H
#include <dumbnet.h>
#else
#include <dnet.h>
#endif

#endif
