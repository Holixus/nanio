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

#include <netinet/ip.h>

#include "nano/io_stream.h"

typedef
union sockaddr {
	sa_family_t family;
	struct sockaddr sa;
	struct sockaddr_un un;
	struct sockaddr_in in;
	struct sockaddr_in6 in6;
} unisa_t;


/* -------------------------------------------------------------------------- */
static char const *sctoa(io_sock_conf const *sc)
{
	switch (sc->family) {
	case AF_INET:
		return ipv4_itoa(sc->addr.ipv4);
	case AF_UNIX:
		return sc->addr.unix;
	case AF_INET6:
		return ipv6_htoa(sc->addr.ipv6);
	}
	return "[BAD_ADDRESS]";
}

/* -------------------------------------------------------------------------- */
static size_t _io_set_addr(unisa_t *sa, io_sock_conf const *conf)
{
	memset(sa, 0, sizeof *sa);
	switch (sa->family = conf->family) {
	case AF_INET:
		sa->in.sin_port = htons(conf->port);
		sa->in.sin_addr.s_addr = htonl(conf->addr.ipv4 ? conf->addr.ipv4 : INADDR_ANY);
		return sizeof sa->in;

	case AF_UNIX:
		strcpy(sa->un.sun_path, conf->addr.unix);
		return sizeof sa->un;

	case AF_INET6:
		sa->in6.sin6_port = htons(conf->port);
		ipv6_hton(sa->in6.sin6_addr.s6_addr, conf->addr.ipv6);
		return sizeof sa.in6;
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static size_t _io_get_addr(io_sock_conf *conf, unisa_t const *sa)
{
	switch (conf->family = sa->family) {
	case AF_INET:
		conf->port = ntohs(sa->in.sin_port);
		conf->addr.ipv4 = ntohl(sa->in.sin_addr.s_addr);
		return sizeof sa->in;

	case AF_UNIX:
		conf->addr.unix = "";//sa->un.sun_path; // accept doesn't returns remote unix path
		return sizeof sa->un;

	case AF_INET6:
		conf->port = ntohs(sa->in6.sin6_port);
		ipv6_ntoh(conf->addr.ipv6, sa->in6.sin6_addr.s6_addr);
		return sizeof sa.in6;
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int _io_stream_listen(io_stream_listen_conf_t *conf)
{
	unisa_t sa;
	size_t sa_size = _io_set_addr(&sa, &conf->sock);

	int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

	if (conf->iface[0] && (sa.family == AF_INET || sa.family == AF_INET6))
		if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, conf->iface, (socklen_t)strlen(conf->iface) + 1) < 0) {
			syslog(LOG_ERR, "fail to bind listen stream_socket to '%s' (%m)", conf->iface);
			close(sock);
			return NULL;
		}

	if (bind(sock, &sa.sa, sa_size) < 0 || listen(sock, conf->queue_size) < 0) {
		syslog(LOG_ERR, "fail to bind listen stream socket to '%s:%d' (%m)", sctoa(&conf->sock), conf->sock.port);
		close(sock);
		return NULL;
	}

	return sock;
}


/* -------------------------------------------------------------------------- */
static int _io_stream_accept(io_sock_t *sock, io_sock_conf_t *sc)
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

	int sock = accept4(sock->fd, &sa.sa, &addrlen, SOCK_NONBLOCK);
	if (sock < 0)
		syslog(LOG_ERR, "fail to accept connection (%m)");
	else
		_io_get_addr(sc, &sa);
	return sock;
}


/* -------------------------------------------------------------------------- */
static int _io_stream_connect(io_stream_conf_t *conf)
{
	unisa_t sa;
	size_t sa_size = _io_set_addr(&sa, &conf->sock);

	int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (connect(sock, &sa.sa, sa_size) < 0 && errno != EINPROGRESS) {
		syslog(LOG_ERR, "fail to connect to %s:%d (%m)", sctoa(&conf->sock), p->host_port);
		close(sock);
		return -1;
	}

	return sock;
}


/* -------------------------------------------------------------------------- */
static void io_stream_listen_event_handler(io_sock_t *sock, int events)
{
	io_stream_listen_t *p = (io_stream_listen_t *)sock;
	p->accept_handler(p);
}

/* -------------------------------------------------------------------------- */
static const io_sock_ops_t io_stream_listen_ops = {
	.free = NULL,
	.idle = NULL,
	.event = io_stream_listen_event_handler
};

/* -------------------------------------------------------------------------- */
io_stream_listen_t *io_stream_listen_create(io_stream_listen_conf_t *conf, stream_accept_handler_t *handler)
{
	int sock = _io_stream_listen(conf);
	if (sock < 0)
		return NULL;

	io_stream_listen_t *self = (io_stream_listen_t *)calloc(1, sizeof (io_stream_listen_t));

	memcpy(self->ip, conf->ip, sizeof self->ip);

	self->accept_handler = handler;

	io_sock_init(&self->sock, sock, POLLIN, &io_stream_listen_ops);
	return self;
}




/* -------------------------------------------------------------------------- */
void io_stream_free(io_sock_t *sock)
{
	io_buf_sock_free(sock);
}

/* -------------------------------------------------------------------------- */
static void io_stream_event_handler(io_sock_t *sock, int events)
{
	if (events & POLLOUT)
		io_buf_sock_event_handler(sock, events);

	if (events & POLLIN) {
		io_buf_stream_t *s = (io_stream_t *)sock;
		if (s->connecting) {
			int err = 0;
			socklen_t len = sizeof (int);
			if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
				errno = err;
				syslog(LOG_ERR, "connect to %s:%d failed (%m)", sctoa(&s->conf.sock), s->conf.sock.port);
				io_stream_free(sock);
			} else {
				if (!err)
					p->connecting = 0;
			}
		} else
			io_buf_sock_event_handler(sock, events);
	}
}


/* -------------------------------------------------------------------------- */
static const io_sock_ops_t io_stream_ops = {
	.free = io_stream_stream_free,
	.idle = NULL,
	.event = io_stream_event_handler
};

/* -------------------------------------------------------------------------- */
io_stream_buf_t *io_stream_accept(io_stream_buf_t *t, io_sock_t *listen_sock)
{
	io_stream_conf_t conf;
	int sock = _io_stream_accept(listen_sock, &conf);
	if (sock < 0)
		return NULL;

	if (!t)
		t = (io_stream_buf_t *)calloc(1, sizeof (io_stream_buf_t));

	t->connecting = 0;
	t->conf = *conf;

	io_buf_sock_create(&t->sock, sock, io_stream_ops);
	return t;
}

/* -------------------------------------------------------------------------- */
io_stream_buf_t *io_stream_connect(io_stream_buf_t *t, io_stream_conf_t *conf)
{
	int sock = _io_stream_connect(conf);
	if (sock < 0)
		return NULL;

	if (!t)
		t = (io_stream_buf_t *)calloc(1, sizeof (io_stream_buf_t));

	t->connecting = 1;
	t->conf = *conf;

	io_buf_sock_create(&t->sock, sock, io_stream_ops);
	return t;
}
