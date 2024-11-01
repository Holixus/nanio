#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#include <errno.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <sys/un.h>

#include <netinet/ip.h>

#include "nano/io.h"
#include "nano/io_log.h"
#include "nano/io_ds.h"

#include "nano/io_sock_addr.h"

#include "nano/io_ipv4.h"
#include "nano/io_ipv6.h"
#include "io_internals.h"

static char const *strf(char const *format, ...) __attribute__ ((format (printf, 1, 2)));

/* ------------------------------------------------------------------------ */
static char const *strf(char const *format, ...)
{
	char *buf = _getTmpStr();
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, TMP_STR_SIZE, format, ap);
	va_end(ap);
	return buf;
}


/* -------------------------------------------------------------------------- */
char const *io_sock_hostoa(io_sock_addr_t const *sc)
{
	switch (sc->family) {
#ifdef IPV4_SOCKET
	case AF_INET:
		return ipv4_itoa(sc->addr.ipv4);
#endif
#ifdef UNIX_SOCKET
	case AF_UNIX:
		return sc->addr.path;
#endif
#ifdef IPV6_SOCKET
	case AF_INET6:
		return ipv6_htoa(sc->addr.ipv6);
#endif
	}
	return strf("[BAD_ADDRESS_FAMILY:%d]", sc->family);
}

/* -------------------------------------------------------------------------- */
char const *io_sock_stoa(io_sock_addr_t const *sc)
{
	switch (sc->family) {
#ifdef IPV4_SOCKET
	case AF_INET:
		return ipv4_stoa(sc->addr.ipv4, sc->port);
#endif
#ifdef UNIX_SOCKET
	case AF_UNIX:
		return sc->addr.path;
#endif
#ifdef IPV6_SOCKET
	case AF_INET6:
		return ipv6_stoa(sc->addr.ipv6, sc->port);
#endif
	}
	return strf("[BAD_ADDRESS_FAMILY:%d]", sc->family);
}

/* -------------------------------------------------------------------------- */
int io_sock_atohost(io_sock_addr_t *host, char const *a)
{
#ifdef UNIX_SOCKET
	if (*a == '/') {
		host->addr.path = a;
		return host->family = AF_UNIX;
	}
#endif
#ifdef IPV4_SOCKET
	if (ipv4_isip(a)) {
		host->addr.ipv4 = ipv4_atoi(a);
		return host->family = AF_INET;
	}
#endif
#ifdef IPV6_SOCKET
	if (ipv6_isip(a)) {
		ipv6_atoh(host->addr.ipv6, a);
		return host->family = AF_INET6;
	}
#endif
	return -1;
}

/* -------------------------------------------------------------------------- */
int io_sock_any(io_sock_addr_t *host, int family, int port)
{
	host->port = port;
	switch (host->family = family) {
#ifdef IPV4_SOCKET
	case AF_INET:
		host->addr.ipv4 = 0;
		return 0;
#endif
#ifdef IPV6_SOCKET
	case AF_INET6:
		memset(host->addr.ipv6, 0, sizeof host->addr.ipv6);
		return 0;
#endif
	}
	return errno = EINVAL, -1;
}

/* -------------------------------------------------------------------------- */
int io_sock_atos(io_sock_addr_t *host, char const *a)
{
#ifdef UNIX_SOCKET
	if (*a == '/') {
		host->addr.path = a;
		return host->family = AF_UNIX;
	}
#endif
#ifdef IPV6_SOCKET
	if (*a == '[') {
		char const *b = strchr(++a, ']');
		//io_info("[..");
		if (!b || b[1] != ':')
			return (errno = EINVAL), -1;
		b += 2;

		char addr[48];
		unsigned int len = b - a;
		memcpy(addr, a, len);
		addr[len] = 0;
		//io_info("[%s]", addr);
		if (!ipv6_atoh(host->addr.ipv6, addr))
			return (errno = EINVAL), -1;

		host->port = atoi(b);
		return host->family = AF_INET6;
	}
#endif
#ifdef IPV4_SOCKET
	if (ipv4_isip(a)) {
		host->addr.ipv4 = ipv4_atoi(a);
		char const *colon = strchr(a, ':');
		if (colon)
			host->port = atoi(colon + 1);
		return host->family = AF_INET;
	}
#endif
	return (errno = EINVAL), -1;
}

/* -------------------------------------------------------------------------- */
size_t io_sock_set_addr(unisa_t *usa, io_sock_addr_t const *sa)
{
	memset(usa, 0, sizeof *usa);
	switch (usa->sa.sa_family = sa->family) {
#ifdef IPV4_SOCKET
	case AF_INET:
		usa->in.sin_port = htons(sa->port);
		usa->in.sin_addr.s_addr = htonl(sa->addr.ipv4 ? sa->addr.ipv4 : INADDR_ANY);
		return sizeof usa->in;
#endif

#ifdef UNIX_SOCKET
	case AF_UNIX:
		strcpy(usa->un.sun_path, sa->addr.path);
		return sizeof usa->un;
#endif

#ifdef IPV6_SOCKET
	case AF_INET6:
		usa->in6.sin6_port = htons(sa->port);
		ipv6_hton(usa->in6.sin6_addr.s6_addr, sa->addr.ipv6);
		return sizeof usa->in6;
#endif
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
size_t io_sock_get_addr(io_sock_addr_t *sa, unisa_t const *usa)
{
	switch (sa->family = usa->sa.sa_family) {
#ifdef IPV4_SOCKET
	case AF_INET:
		sa->port = ntohs(usa->in.sin_port);
		sa->addr.ipv4 = ntohl(usa->in.sin_addr.s_addr);
		return sizeof usa->in;
#endif

#ifdef UNIX_SOCKET
	case AF_UNIX:
		sa->addr.path = "";//usa->un.sun_path; // accept doesn't returns remote unix path
		return sizeof usa->un;
#endif

#ifdef IPV6_SOCKET
	case AF_INET6:
		sa->port = ntohs(usa->in6.sin6_port);
		ipv6_ntoh(sa->addr.ipv6, usa->in6.sin6_addr.s6_addr);
		return sizeof usa->in6;
#endif
	default:
		io_err("%s(): bad address family %d", __FUNCTION__, usa->sa.sa_family);
	}
	return 0;
}


/* -------------------------------------------------------------------------- */
int io_socket(int type, int family, char const *iface)
{
	int sd = socket(family, type | SOCK_NONBLOCK, 0);

	if (iface[0] && (family == AF_INET || family == AF_INET6))
#ifndef __KOS__
		if (setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, iface, (socklen_t)strlen(iface) + 1) < 0) {
			io_err("fail to bind listen socket to '%s'", iface);
			close(sd);
			return -1;
		}
#else
	{
		io_err("bind listen socket to interface('%s') isn't supported", iface);
		close(sd);
		errno = ENOTSUP;
		return -1;
	}
#endif

	return sd;
}


/* -------------------------------------------------------------------------- */
int io_binded_socket(int type, io_sock_addr_t *sa, char const *iface)
{
	int sd = io_socket(type, sa->family, iface);

	int on;
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		io_err("Set socket option: SO_REUSEADDR fails '%s'", io_sock_stoa(sa));
		close(sd);
		return -1;
	}

	unisa_t usa;
	size_t sa_size = io_sock_set_addr(&usa, sa);
	if (bind(sd, &usa.sa, sa_size) < 0) {
		io_err("fail to bind listen socket to '%s'", io_sock_stoa(sa));
		close(sd);
		return -1;
	}

	return sd;
}

