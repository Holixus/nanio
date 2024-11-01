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

#include "nano/io_dgram.h"

#include "nano/io_ipv4.h"
#include "nano/io_ipv6.h"


/* -------------------------------------------------------------------------- */
int io_dgram_send(io_d_t *d, io_sock_addr_t const *sa, uint8_t const *buf, size_t size);
{
	unisa_t usa;
	return sendto(d->fd, buf, size, 0, &usa, io_sock_set_addr(&usa, sa));
}


/* -------------------------------------------------------------------------- */
int io_dgram_recv(io_d_t *d, io_sock_addr_t *sa, uint8_t const *buf, size_t size);
{
	unisa_t usa;
	size_t sa_size;
	int r = recvfrom(d->fd, buf, size, 0, &usa, &sa_size)
	if (r >= 0)
		io_sock_get_addr(sa, &usa);
	return r;
}


/* -------------------------------------------------------------------------- */
int io_dgram_serv_con_send(io_dgram_serv_con_t *d, uint8_t const *buf, size_t size)
{
	return io_dgram_send(d->server.d.fd, &d->sa, buf, size);
}

/* -------------------------------------------------------------------------- */
int io_dgram_client_send(io_dgram_client_t *d, uint8_t const *buf, size_t size)
{
	return io_dgram_send(d->d.fd, &d->sa, buf, size);
}


/* -------------------------------------------------------------------------- */
static void io_dgram_server_event_handler(io_d_t *iod, int events)
{
	io_dgram_server_t *p = (io_dgram_server_t *)iod;

	io_sock_addr_t addr;
	char buf[1500];

	ssize_t recvd = io_dgram_recv(iod, &addr, buf, sizeof buf);

	p->recv_handler(p, &addr, buf, recvd);
}


/* -------------------------------------------------------------------------- */
static const io_d_ops_t io_dgram_server_ops = {
	.free = NULL,
	.idle = NULL,
	.event = io_dgram_server_event_handler
};

/* -------------------------------------------------------------------------- */
io_dgram_server_t *io_dgram_server_create(io_dgram_server_conf_t *conf, io_dgram_server_handler_t *handler)
{
	int sd = io_binded_socket(SOCK_DGRAM, &conf->sd, conf->iface);
	if (sd < 0)
		return NULL;

	io_dgram_server_t *self = (io_dgram_server_t *)calloc(1, sizeof (io_dgram_server_t));

	self->sa = conf->sa;
	self->recv_handler = handler;

	io_d_init(&self->d, sd, POLLIN, &io_dgram_server_ops);
	return self;
}


/* -------------------------------------------------------------------------- */
void io_stream_event_handler(io_d_t *iod, int events)
{
	if (events & POLLOUT)
		io_buf_d_event_handler(iod, events);

	if (events & POLLIN) {
		io_stream_t *s = (io_stream_t *)iod;
		if (s->connecting) {
			int err = 0;
			socklen_t len = sizeof (int);
			if (getsockopt(iod->fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
				errno = err;
				syslog(LOG_ERR, "connect to %s:%d failed (%m)", io_sock_hostoa(&s->sa), s->sa.port);
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
io_stream_t *io_stream_accept(io_stream_t *t, io_stream_listen_t *s, io_event_handler_t *handler)
{
	io_sock_addr_t sa;
	int sd;

	if (s->sa.family == AF_UNIX) {
		sd = accept4(s->d.fd, NULL, NULL, SOCK_NONBLOCK);
	} else {
		unisa_t usa;
		memset(&sa, 0, sizeof sa);
		socklen_t addrlen = sizeof sa;
		sd = accept4(s->d.fd, &usa.sa, &addrlen, SOCK_NONBLOCK);
		if (sd >= 0)
			io_sock_get_addr(&sa, &usa);
	}

	if (sd < 0) {
		syslog(LOG_ERR, "fail to accept connection (%m)");
		return NULL;
	}

	if (!t)
		t = (io_stream_t *)calloc(1, sizeof (io_stream_t));

	if (s->sa.family == AF_UNIX)
		memset(&t->sa, 0, sizeof t->sa);
	else
		t->sa = sa;

	t->connecting = 0;
	t->pollin = handler;

	io_buf_d_create(&t->bd, sd, &io_stream_ops);
	return t;
}

/* -------------------------------------------------------------------------- */
io_stream_t *io_stream_connect(io_stream_t *t, io_sock_addr_t *sa, io_event_handler_t *handler)
{
	int fd = _io_stream_connect(sa);
	if (fd < 0)
		return NULL;

	if (!t)
		t = (io_stream_t *)calloc(1, sizeof (io_stream_t));

	t->connecting = 1;
	t->sa = *sa;
	t->pollin = handler;

	io_buf_d_create(&t->bd, fd, &io_stream_ops);
	return t;
}
