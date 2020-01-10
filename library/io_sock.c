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

#include "nano/io_sock.h"

#include "nano/io_ipv4.h"
#include "nano/io_ipv6.h"

typedef
union _sockaddr {
	sa_family_t family;
	struct sockaddr sa;
	struct sockaddr_un un;
	struct sockaddr_in in;
	struct sockaddr_in6 in6;
} unisa_t;


/* -------------------------------------------------------------------------- */
static char const *hostoa(io_sock_addr_t const *sc)
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
static size_t _io_set_addr(unisa_t *sa, io_sock_addr_t const *conf)
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
static size_t _io_get_addr(io_sock_addr_t *conf, unisa_t const *sa)
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

/* -------------------------------------------------------------------------- */
static int _io_sock_listen(io_sock_listen_conf_t *conf)
{
	unisa_t sa;
	size_t sa_size = _io_set_addr(&sa, &conf->sock);

	int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

	if (conf->iface[0] && (sa.family == AF_INET || sa.family == AF_INET6))
		if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, conf->iface, (socklen_t)strlen(conf->iface) + 1) < 0) {
			syslog(LOG_ERR, "fail to bind listen socket to '%s' (%m)", conf->iface);
			close(sock);
			return -1;
		}

	if (bind(sock, &sa.sa, sa_size) < 0 || listen(sock, conf->queue_size) < 0) {
		syslog(LOG_ERR, "fail to bind listen socket to '%s:%d' (%m)", hostoa(&conf->sock), conf->sock.port);
		close(sock);
		return -1;
	}

	return sock;
}


/* -------------------------------------------------------------------------- */
static int _io_sock_accept(io_d_t *sock, io_sock_addr_t *sc)
{
/*	if (!sc) {
		int sock = accept4(sock->fd, NULL, NULL, SOCK_NONBLOCK);
		if (sock < 0)
			syslog(LOG_ERR, "fail to accept connection (%m)");
		return sock;
	}
*/
	unisa_t sa;
	memset(&sa, 0, sizeof sa);
	socklen_t addrlen = sizeof sa;

	int sd = accept4(sock->fd, &sa.sa, &addrlen, SOCK_NONBLOCK);
	if (sd < 0)
		syslog(LOG_ERR, "fail to accept connection (%m)");
	else
		_io_get_addr(sc, &sa);
	return sd;
}


/* -------------------------------------------------------------------------- */
static int _io_sock_connect(io_sock_addr_t *conf)
{
	unisa_t sa;
	size_t sa_size = _io_set_addr(&sa, conf);

	int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (connect(sock, &sa.sa, sa_size) < 0 && errno != EINPROGRESS) {
		syslog(LOG_ERR, "fail to connect to %s:%d (%m)", hostoa(conf), conf->port);
		close(sock);
		return -1;
	}

	return sock;
}


/* -------------------------------------------------------------------------- */
static void io_sock_listen_event_handler(io_d_t *sock, int events)
{
	io_sock_listen_t *p = (io_sock_listen_t *)sock;

	io_sock_addr_t sc;
	int sd = _io_sock_accept(sock, &sc);
	if (sd >= 0)
		p->accept_handler(p, sd, &sc);
}

/* -------------------------------------------------------------------------- */
static const io_d_ops_t io_sock_listen_ops = {
	.free = NULL,
	.idle = NULL,
	.event = io_sock_listen_event_handler
};

/* -------------------------------------------------------------------------- */
io_sock_listen_t *io_sock_listen_create(io_sock_listen_conf_t *conf, sock_accept_handler_t *handler)
{
	int sock = _io_sock_listen(conf);
	if (sock < 0)
		return NULL;

	io_sock_listen_t *self = (io_sock_listen_t *)calloc(1, sizeof (io_sock_listen_t));

	self->conf = conf->sock;
	self->accept_handler = handler;

	io_d_init(&self->sock, sock, POLLIN, &io_sock_listen_ops);
	return self;
}




/* -------------------------------------------------------------------------- */
void io_sock_free(io_d_t *sock)
{
	io_buf_d_free(sock);
}

/* -------------------------------------------------------------------------- */
static void io_sock_event_handler(io_d_t *sock, int events)
{
	if (events & POLLOUT)
		io_buf_d_event_handler(sock, events);

	if (events & POLLIN) {
		io_buf_sock_t *s = (io_buf_sock_t *)sock;
		if (s->connecting) {
			int err = 0;
			socklen_t len = sizeof (int);
			if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
				errno = err;
				syslog(LOG_ERR, "connect to %s:%d failed (%m)", hostoa(&s->conf), s->conf.port);
				io_d_free(sock);
			} else {
				if (!err)
					s->connecting = 0;
			}
		} else
			io_buf_d_event_handler(sock, events);
	}
}


/* -------------------------------------------------------------------------- */
static const io_d_ops_t io_sock_ops = {
	.free = io_sock_free,
	.idle = NULL,
	.event = io_sock_event_handler
};

/* -------------------------------------------------------------------------- */
io_buf_sock_t *io_sock_accept(io_buf_sock_t *t, io_d_t *listen_sock)
{
	io_sock_addr_t conf;
	int sock = _io_sock_accept(listen_sock, &conf);
	if (sock < 0)
		return NULL;

	if (!t)
		t = (io_buf_sock_t *)calloc(1, sizeof (io_buf_sock_t));

	t->connecting = 0;
	t->conf = conf;

	io_buf_d_create(&t->sock, sock, &io_sock_ops);
	return t;
}

/* -------------------------------------------------------------------------- */
io_buf_sock_t *io_sock_connect(io_buf_sock_t *t, io_sock_addr_t *conf)
{
	int sock = _io_sock_connect(conf);
	if (sock < 0)
		return NULL;

	if (!t)
		t = (io_buf_sock_t *)calloc(1, sizeof (io_buf_sock_t));

	t->connecting = 1;
	t->conf = *conf;

	io_buf_d_create(&t->sock, sock, &io_sock_ops);
	return t;
}
