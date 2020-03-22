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
static int io_stream_listen_event_handler(io_d_t *iod, int events)
{
	return iod->vmt->u.stream.accept(iod);
}


/* -------------------------------------------------------------------------- */
io_vmt_t io_stream_listen_vmt = {
	.name = "io_stream_listen",
	.ancestor = &io_d_vmt,
	.event = io_stream_listen_event_handler
};


/* -------------------------------------------------------------------------- */
io_stream_listen_t *io_stream_listen_create(io_stream_listen_t *self, io_stream_listen_conf_t *conf, io_vmt_t *vmt)
{
	int sock = io_binded_socket(SOCK_STREAM, &conf->sock, conf->iface);
	if (sock < 0)
		return NULL;

	if (listen(sock, conf->queue_size) < 0) {
		syslog(LOG_ERR, "fail to listen socket to '%s' (%m)", io_sock_stoa(&conf->sock));
		close(sock);
		return NULL;
	}

	if (!self)
		self = (io_stream_listen_t *)calloc(1, sizeof (io_stream_listen_t));

	self->conf = conf->sock;

	io_d_init(&self->d, sock, POLLIN, vmt);
	return self;
}


/* -------------------------------------------------------------------------- */
int io_stream_event_handler(io_d_t *iod, int events)
{
	if (events & POLLOUT)
		if (io_buf_d_event_handler(iod, events) < 0)
			return -1;

	if (events & POLLIN) {
		io_buf_sock_t *s = (io_buf_sock_t *)iod;
		if (s->connecting) {
			int err = 0;
			socklen_t len = sizeof (int);
			if (getsockopt(iod->fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
				errno = err;
				syslog(LOG_ERR, "connect to %s:%d failed (%m)", io_sock_hostoa(&s->conf), s->conf.port);
				io_d_free(iod);
				return -1;
			} else {
				if (!err)
					s->connecting = 0;
				else
					return 0; // not connected yet
			}
		}

		if (iod->vmt->u.stream.recv) {
			if (iod->vmt->u.stream.recv(iod) < 0) {
				io_d_free(iod);
				return -1;
			}
		}
	}
	return 0;
}


/* -------------------------------------------------------------------------- */
void io_stream_free(io_d_t *d)
{
	if (d->vmt->u.stream.close)
		d->vmt->u.stream.close(d);
	if (d->vmt->ancestor->u.stream.close)
		d->vmt->ancestor->u.stream.close(d);
}


/* -------------------------------------------------------------------------- */
io_vmt_t io_stream_vmt = {
	.name = "io_stream",
	.ancestor = &io_d_vmt,
	.free = io_stream_free,
	.event = io_stream_event_handler
};


/* -------------------------------------------------------------------------- */
io_buf_sock_t *io_stream_accept(io_buf_sock_t *t, io_stream_listen_t *s, io_vmt_t *vmt)
{
	io_sock_addr_t conf;
	int sd;

	if (s->conf.family == AF_UNIX) {
		sd = accept4(s->d.fd, NULL, NULL, SOCK_NONBLOCK);
	} else {
		unisa_t sa;
		memset(&sa, 0, sizeof sa);
		socklen_t addrlen = sizeof sa;
		sd = accept4(s->d.fd, &sa.sa, &addrlen, SOCK_NONBLOCK);
		if (sd >= 0)
			io_sock_get_addr(&conf, &sa);
	}

	if (sd < 0) {
		syslog(LOG_ERR, "fail to accept connection (%m)");
		return NULL;
	}

	if (!t)
		t = (io_buf_sock_t *)calloc(1, sizeof (io_buf_sock_t));

	if (s->conf.family == AF_UNIX)
		memset(&t->conf, 0, sizeof t->conf);
	else
		t->conf = conf;

	t->connecting = 0;

	io_buf_d_create(&t->bd, sd, vmt ?: &io_stream_vmt);
	return t;
}

/* -------------------------------------------------------------------------- */
io_buf_sock_t *io_stream_connect(io_buf_sock_t *t, io_sock_addr_t *conf, io_vmt_t *vmt)
{
	int fd = _io_stream_connect(conf);
	if (fd < 0)
		return NULL;

	if (!t)
		t = (io_buf_sock_t *)calloc(1, sizeof (io_buf_sock_t));

	t->connecting = 1;
	t->conf = *conf;

	io_buf_d_create(&t->bd, fd, vmt);
	return t;
}
