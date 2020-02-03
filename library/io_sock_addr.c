#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#include <errno.h>
#include <syslog.h>

#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <sys/un.h>

#include <netinet/ip.h>

#include "nano/io.h"

#include "nano/io_ds.h"
#include "nano/io_buf.h"
#include "nano/io_buf_d.h"

#include "nano/io_sock_addr.h"

#include "nano/io_ipv4.h"
#include "nano/io_ipv6.h"


/* -------------------------------------------------------------------------- */
char const *io_sock_hostoa(io_sock_addr_t const *sc)
{
	switch (sc->family) {
	case AF_INET:
		return ipv4_itoa(sc->addr.ipv4);
	case AF_UNIX:
		return sc->addr.path;
	case AF_INET6:
		return ipv6_htoa(sc->addr.ipv6);
	}
	return "[BAD_ADDRESS]";
}

/* -------------------------------------------------------------------------- */
char const *io_sock_stoa(io_sock_addr_t const *sc)
{
	switch (sc->family) {
	case AF_INET:
		return ipv4_stoa(sc->addr.ipv4, sc->port);
	case AF_UNIX:
		return sc->addr.path;
	case AF_INET6:
		return ipv6_stoa(sc->addr.ipv6, sc->port);
	}
	return "[BAD_ADDRESS]";
}

/* -------------------------------------------------------------------------- */
int io_sock_atohost(io_sock_addr_t *host, char const *a)
{
	if (*a == '/') {
		host->addr.path = a;
		return host->family = AF_UNIX;
	}
	if (ipv4_isip(a)) {
		host->addr.ipv4 = ipv4_atoi(a);
		return host->family = AF_INET;
	}
	if (ipv6_isip(a)) {
		ipv6_atoh(host->addr.ipv6, a);
		return host->family = AF_INET6;
	}

	return -1;
}

/* -------------------------------------------------------------------------- */
int io_sock_atos(io_sock_addr_t *host, char const *a)
{
	if (*a == '/') {
		host->addr.path = a;
		return host->family = AF_UNIX;
	}
	if (*a == '[') {
		char const *b = strchr(++a, ']');
		syslog(LOG_NOTICE, "[..");
		if (!b || b[1] != ':')
			return -1;
		b += 2;

		char addr[48];
		unsigned int len = b - a;
		memcpy(addr, a, len);
		addr[len] = 0;
		syslog(LOG_NOTICE, "[%s]", addr);
		if (!ipv6_atoh(host->addr.ipv6, addr))
			return -1;

		host->port = atoi(b);
		return host->family = AF_INET6;
	}
	if (ipv4_isip(a)) {
		host->addr.ipv4 = ipv4_atoi(a);
		char const *colon = strchr(a, ':');
		if (colon)
			host->port = atoi(colon + 1);
		return host->family = AF_INET;
	}
	return (errno = EINVAL), -1;
}

/* -------------------------------------------------------------------------- */
size_t io_sock_set_addr(unisa_t *sa, io_sock_addr_t const *conf)
{
	memset(sa, 0, sizeof *sa);
	switch (sa->family = conf->family) {
	case AF_INET:
		sa->in.sin_port = htons(conf->port);
		sa->in.sin_addr.s_addr = htonl(conf->addr.ipv4 ? conf->addr.ipv4 : INADDR_ANY);
		return sizeof sa->in;

	case AF_UNIX:
		strcpy(sa->un.sun_path, conf->addr.path);
		return sizeof sa->un;

	case AF_INET6:
		sa->in6.sin6_port = htons(conf->port);
		ipv6_hton(sa->in6.sin6_addr.s6_addr, conf->addr.ipv6);
		return sizeof sa->in6;
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
size_t io_sock_get_addr(io_sock_addr_t *conf, unisa_t const *sa)
{
	switch (conf->family = sa->family) {
	case AF_INET:
		conf->port = ntohs(sa->in.sin_port);
		conf->addr.ipv4 = ntohl(sa->in.sin_addr.s_addr);
		return sizeof sa->in;

	case AF_UNIX:
		conf->addr.path = "";//sa->un.sun_path; // accept doesn't returns remote unix path
		return sizeof sa->un;

	case AF_INET6:
		conf->port = ntohs(sa->in6.sin6_port);
		ipv6_ntoh(conf->addr.ipv6, sa->in6.sin6_addr.s6_addr);
		return sizeof sa->in6;
	}
	return 0;
}

