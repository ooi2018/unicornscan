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
#include <config.h>

#include <errno.h>

#include <settings.h>
#include <unilib/xmalloc.h>
#include <unilib/output.h>
#include <unilib/xpoll.h>

/*
XXX add epoll support and perhaps a wrapper for RT IO etc
XXX make dynamic not bounded
*/

#if defined(HAVE_KQUEUE) && defined(HAVE_KEVENT)

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * kqueue-based xpoll implementation for macOS/BSD.
 *
 * Design decisions:
 * - A static kqueue fd is used and persisted across calls to avoid the
 *   overhead of creating/destroying it each time. It is created lazily
 *   on first use.
 * - Each call builds a fresh changelist with EV_ADD for all descriptors.
 *   This matches the poll() semantics where the set of watched fds is
 *   specified anew each call (callers do not maintain persistent watch
 *   state).
 * - We register EVFILT_READ for each fd (equivalent to POLLIN).
 *   kqueue does not have a direct POLLPRI equivalent; OOB data on
 *   macOS is delivered via EVFILT_READ with the EV_OOBAND flag, but
 *   in practice callers never check XPOLL_PRIREADABLE independently
 *   of XPOLL_READABLE. We set XPOLL_PRIREADABLE when EV_OOBAND is
 *   reported for completeness.
 * - POLLHUP maps to EV_EOF flag on the returned event.
 * - POLLERR maps to EV_ERROR flag on the returned event.
 * - POLLNVAL (invalid fd) is detected when kevent returns EV_ERROR
 *   with errno EBADF for that fd in the changelist registration.
 * - Timeout conversion: poll() uses int milliseconds (-1 = infinite,
 *   0 = immediate). kevent() uses struct timespec* (NULL = infinite).
 * - EINTR retry matches the poll() version behavior.
 */

/* Persistent kqueue file descriptor, created once on first use. */
static int kq_fd=-1;

/*
 * get_kqueue_fd: lazily create the kqueue descriptor.
 * Returns the kqueue fd, or -1 on failure.
 * Sets close-on-exec to prevent leaking into child processes.
 */
static int get_kqueue_fd(void) {
	if (kq_fd != -1) {
		return kq_fd;
	}

	kq_fd=kqueue();
	if (kq_fd < 0) {
		ERR("kqueue create fails: %s", strerror(errno));
		return -1;
	}

	/* Set close-on-exec so forked children don't inherit the kqueue fd */
	if (fcntl(kq_fd, F_SETFD, FD_CLOEXEC) < 0) {
		ERR("fcntl FD_CLOEXEC on kqueue fd fails: %s", strerror(errno));
		/* Non-fatal: continue without close-on-exec */
	}

	return kq_fd;
}

int xpoll(xpoll_t *array, uint32_t len, int timeout) {
	uint32_t j=0, idx=0;
	int ret=0, nev=0, kqfd=-1;
	xpoll_t *start=NULL;
	struct kevent changelist[MAX_CONNS];
	struct kevent eventlist[MAX_CONNS];
	struct timespec ts, *tsp=NULL;

	assert(array != NULL);
	assert(len < MAX_CONNS);

	kqfd=get_kqueue_fd();
	if (kqfd < 0) {
		return -1;
	}

	/*
	 * Build the changelist: register EVFILT_READ on each fd.
	 * EV_ADD adds or modifies the filter. EV_ONESHOT is NOT used
	 * so we match poll() semantics where fds remain watchable.
	 * We clear the output flags for each xpoll_t entry.
	 */
	for (j=0, start=array; j < len; j++, array++) {
		EV_SET(&changelist[j], array->fd, EVFILT_READ,
			EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, (void *)(uintptr_t)j);
		array->rw=0;
	}

	/*
	 * Convert timeout:
	 * poll() semantics: -1 = block forever, 0 = return immediately,
	 * >0 = milliseconds to wait.
	 * kevent() semantics: NULL = block forever, &ts = wait that long.
	 */
	if (timeout < 0) {
		tsp=NULL; /* block indefinitely */
	}
	else {
		ts.tv_sec=timeout / 1000;
		ts.tv_nsec=(timeout % 1000) * 1000000L;
		tsp=&ts;
	}

rekqueue:
	nev=kevent(kqfd, &changelist[0], (int)len, &eventlist[0], (int)len, tsp);
	if (nev < 0) {
		if (errno == EINTR) {
			goto rekqueue;
		}
		ERR("kevent errors: %s", strerror(errno));
		return -1;
	}

	/*
	 * Process the returned events.
	 *
	 * kevent returns only the fds that had activity (unlike poll which
	 * returns the full array). We need to map events back to the
	 * original xpoll_t array positions. We stored the index in the
	 * udata field of each changelist entry for exactly this purpose.
	 *
	 * Note: if a changelist entry has an error (e.g., EBADF for an
	 * invalid fd), kevent reports it as an event with EV_ERROR set
	 * and the error code in the data field. This is the POLLNVAL
	 * equivalent.
	 */
	ret=nev;
	array=start;

	for (j=0; j < (uint32_t)nev; j++) {

		idx=(uint32_t)(uintptr_t)eventlist[j].udata;
		assert(idx < len);

		if (eventlist[j].flags & EV_ERROR) {
			/*
			 * EV_ERROR means the changelist registration failed
			 * for this fd. The data field contains the errno.
			 * This covers POLLNVAL (EBADF) and other fd errors.
			 */
			array[idx].rw |= XPOLL_DEAD;
			DBG(M_PIO, "Socket %d kqueue EV_ERROR: %s",
				(int)eventlist[j].ident,
				strerror((int)eventlist[j].data));
			continue;
		}

		if (eventlist[j].flags & EV_EOF) {
			/*
			 * EV_EOF indicates the peer disconnected or the
			 * connection was reset. This is the POLLHUP equivalent.
			 * The fflags field may contain additional error info.
			 * Note: EV_EOF can be set alongside readable data
			 * (data still in the socket buffer after peer close),
			 * so we set both DEAD and READABLE when appropriate.
			 */
			array[idx].rw |= XPOLL_DEAD;

			if (eventlist[j].fflags != 0) {
				DBG(M_PIO, "Socket %d EOF with error: %s",
					(int)eventlist[j].ident,
					strerror((int)eventlist[j].fflags));
			}
		}

		if (eventlist[j].filter == EVFILT_READ) {
			/*
			 * Data available for reading. eventlist[j].data
			 * contains the number of bytes available.
			 * Even if EV_EOF is set, there may be remaining
			 * data to read, so we still mark it readable if
			 * data > 0 or if EV_EOF is not set.
			 */
			if (eventlist[j].data > 0 || !(eventlist[j].flags & EV_EOF)) {
				array[idx].rw |= XPOLL_READABLE;
			}

			/*
			 * Check for OOB/priority data (NOTE_OOB on macOS).
			 * This is the closest kqueue equivalent to POLLPRI.
			 */
#ifdef NOTE_OOB
			if (eventlist[j].fflags & NOTE_OOB) {
				array[idx].rw |= XPOLL_PRIREADABLE;
			}
#endif
		}

		DBG(M_PIO, "Socket %d is %s %s %s",
			(int)eventlist[j].ident,
			(array[idx].rw & XPOLL_DEAD ? "dead" : "alive"),
			(array[idx].rw & XPOLL_READABLE ? "readable" : "not readable"),
			(array[idx].rw & XPOLL_PRIREADABLE ? "pri-readable" : "not pri-readable")
		);
	}

	return ret;
}

#else /* !HAVE_KQUEUE || !HAVE_KEVENT -- use poll() fallback */

#include <poll.h>
#include <sys/poll.h>

int xpoll(xpoll_t *array, uint32_t len, int timeout) {
	uint32_t j=0;
	int ret=0;
	xpoll_t *start=NULL;
	struct pollfd pdf[MAX_CONNS];

	assert(array != NULL);
	assert(len < MAX_CONNS);

	for (j=0, start=array; j < len; j++, array++) {
		pdf[j].fd=array->fd;
		array->rw=0;
		pdf[j].revents=0;
		pdf[j].events=POLLIN|POLLPRI;
	}

repoll:
	if ((ret=poll(&pdf[0], len, timeout)) < 0) {
		if (ret < 0 && errno == EINTR) {
			goto repoll;
		}
		ERR("poll errors: %s", strerror(errno));
		return -1;
	}

	for (array=start, j=0; j < len; j++, array++) {
		array->rw=0;
		if (pdf[j].revents & (POLLHUP|POLLERR|POLLNVAL)) {
			array->rw |= XPOLL_DEAD;
		}
		if (pdf[j].revents & POLLIN) {
			array->rw |= XPOLL_READABLE;
		}
		if (pdf[j].revents & POLLPRI) {
			array->rw |= XPOLL_PRIREADABLE;
		}
		DBG(M_PIO, "Socket %d is %s %s %s", pdf[j].fd,
			(array->rw & XPOLL_DEAD ? "dead" : "alive"),
			(array->rw & XPOLL_READABLE ? "readable" : "not readable"),
			(array->rw & XPOLL_PRIREADABLE ? "pri-readable" : "not pri-readable")
		);
	}

	return ret;
}

#endif /* HAVE_KQUEUE && HAVE_KEVENT */
