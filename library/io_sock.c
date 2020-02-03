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

#include "nano/io_stream.h"

#include "nano/io_ipv4.h"
#include "nano/io_ipv6.h"

/* -------------------------------------------------------------------------- */
static int _io_stream_listen(io_stream_listen_conf_t *conf)
{
	unisa_t sa;
	size_t sa_size = io_sock_set_addr(&sa, &conf->sock);

	int sock = socket(conf->sock.family, SOCK_STREAM | SOCK_NONBLOCK, 0);

	if (conf->iface[0] && (sa.family == AF_INET || sa.family == AF_INET6))
		if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, conf->iface, (socklen_t)strlen(conf->iface) + 1) < 0) {
			syslog(LOG_ERR, "fail to bind listen socket to '%s' (%m)", conf->iface);
			close(sock);
			return -1;
		}

	int on;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		syslog(LOG_ERR, "Set socket option: SO_REUSEADDR fails '%s' (%m)", io_sock_stoa(&conf->sock));
		exit(1);
	}

	if (bind(sock, &sa.sa, sa_size) < 0 || listen(sock, conf->queue_size) < 0) {
		syslog(LOG_ERR, "fail to bind listen socket to '%s' (%m)", io_sock_stoa(&conf->sock));
		close(sock);
		return -1;
	}

	return sock;
}


/* -------------------------------------------------------------------------- */
static int _io_stream_accept(io_d_t *sock, io_sock_addr_t *sc)
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
		io_sock_get_addr(sc, &sa);
	return sd;
}


/* -------------------------------------------------------------------------- */
static int _io_stream_connect(io_sock_addr_t *conf)
{
	unisa_t sa;
	size_t sa_size = io_sock_set_addr(&sa, conf);

	int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (connect(sock, &sa.sa, sa_size) < 0 && errno != EINPROGRESS) {
		syslog(LOG_ERR, "fail to connect to %s:%d (%m)", io_sock_hostoa(conf), conf->port);
		close(sock);
		return -1;
	}

	return sock;
}


/* -------------------------------------------------------------------------- */
static void io_stream_listen_event_handler(io_d_t *iod, int events)
{
	io_stream_listen_t *p = (io_stream_listen_t *)iod;
	p->accept_handler(p);
}

/* -------------------------------------------------------------------------- */
static const io_d_ops_t io_stream_listen_ops = {
	.free = NULL,
	.idle = NULL,
	.event = io_stream_listen_event_handler
};

/* -------------------------------------------------------------------------- */
io_stream_listen_t *io_stream_listen_create(io_stream_listen_conf_t *conf, io_stream_accept_handler_t *handler)
{
	int sock = _io_stream_listen(conf);
	if (sock < 0)
		return NULL;

	io_stream_listen_t *self = (io_stream_listen_t *)calloc(1, sizeof (io_stream_listen_t));

	self->conf = conf->sock;
	self->accept_handler = handler;

	io_d_init(&self->d, sock, POLLIN, &io_stream_listen_ops);
	return self;
}


/* -------------------------------------------------------------------------- */
void io_stream_event_handler(io_d_t *iod, int events)
{
	if (events & POLLOUT)
		io_buf_d_event_handler(iod, events);

	if (events & POLLIN) {
		io_buf_sock_t *s = (io_buf_sock_t *)iod;
		if (s->connecting) {
			int err = 0;
			socklen_t len = sizeof (int);
			if (getsockopt(iod->fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
				errno = err;
				syslog(LOG_ERR, "connect to %s:%d failed (%m)", io_sock_hostoa(&s->conf), s->conf.port);
				io_d_free(iod);
			} else {
				if (!err)
					s->connecting = 0;
			}
		} else
			io_buf_d_event_handler(iod, events);

		if (s->pollin)
			s->pollin(iod, events);
	}
}


/* -------------------------------------------------------------------------- */
void io_stream_free(io_d_t *d)
{
	io_buf_d_free(d);
}

/* -------------------------------------------------------------------------- */
static const io_d_ops_t io_stream_ops = {
	.free = io_stream_free,
	.idle = NULL,
	.event = io_stream_event_handler
};

/* -------------------------------------------------------------------------- */
io_buf_sock_t *io_stream_accept(io_buf_sock_t *t, io_stream_listen_t *s, io_event_handler_t *handler)
{
	io_sock_addr_t conf;
	int fd = _io_stream_accept(&s->d, &conf);
	if (fd < 0)
		return NULL;

	if (!t)
		t = (io_buf_sock_t *)calloc(1, sizeof (io_buf_sock_t));

	t->connecting = 0;
	t->conf = conf;
	t->pollin = handler;

	io_buf_d_create(&t->bd, fd, &io_stream_ops);
	return t;
}

/* -------------------------------------------------------------------------- */
io_buf_sock_t *io_stream_connect(io_buf_sock_t *t, io_sock_addr_t *conf, io_event_handler_t *handler)
{
	int fd = _io_stream_connect(conf);
	if (fd < 0)
		return NULL;

	if (!t)
		t = (io_buf_sock_t *)calloc(1, sizeof (io_buf_sock_t));

	t->connecting = 1;
	t->conf = *conf;
	t->pollin = handler;

	io_buf_d_create(&t->bd, fd, &io_stream_ops);
	return t;
}
