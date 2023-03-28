#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
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

#include "nano/io_stream.h"

#include "nano/io_ipv4.h"
#include "nano/io_ipv6.h"


/* -------------------------------------------------------------------------- */
static int _io_stream_connect(io_sock_addr_t *sa)
{
	unisa_t usa;
	size_t sa_size = io_sock_set_addr(&usa, sa);

	int sd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (connect(sd, &usa.sa, sa_size) < 0 && errno != EINPROGRESS) {
		syslog(LOG_ERR, "fail to connect to %s:%d (%m)", io_sock_hostoa(sa), sa->port);
		close(sd);
		return -1;
	}

	return sd;
}


/* -------------------------------------------------------------------------- */
int io_stream_write(io_stream_t *s, void const *data, size_t size)
{
	io_d_t *d = &s->d;
	ssize_t sent = 0;

	uint8_t *dt = (uint8_t *)data;

	if (io_buf_is_empty(&s->out)) {
		sent = send(d->fd, dt, size, 0);
		if (sent < 0)
			return -1;
		dt += sent;
		size -= sent;
	}
	d->events |= POLLOUT; // later call all_sent() method
	return size ? io_buf_write(&s->out, dt, size) + sent : sent;
}


/* -------------------------------------------------------------------------- */
int io_stream_vwritef(io_stream_t *s, char const *fmt, va_list ap)
{
	char msg[1024];
	size_t len = (size_t)vsnprintf(msg, sizeof msg, fmt, ap);
	return io_stream_write(s, msg, len);
}


/* -------------------------------------------------------------------------- */
int io_stream_writef(io_stream_t *s, char const *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = io_stream_vwritef(s, fmt, ap);
	va_end(ap);
	return r;
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
	int sd = io_binded_socket(SOCK_STREAM, &conf->sa, conf->iface);
	if (sd < 0)
		return NULL;

	if (listen(sd, conf->queue_size) < 0) {
		syslog(LOG_ERR, "fail to listen socket to '%s' (%m)", io_sock_stoa(&conf->sa));
		close(sd);
		return NULL;
	}

	if (!self)
		self = (io_stream_listen_t *)calloc(1, sizeof (io_stream_listen_t));

	self->sa = conf->sa;

	io_d_init(&self->d, sd, POLLIN, vmt);
	return self;
}


/* -------------------------------------------------------------------------- */
int io_stream_event_handler(io_d_t *iod, int events)
{
	io_stream_t *s = (io_stream_t *)iod;
	if (events & POLLOUT) {
		if (io_buf_send(&s->out, iod->fd) < 0) {
			if (errno == ECONNRESET || errno == ENOTCONN || errno == EPIPE) {
				io_d_free(iod);
			} else
				return -1;
		}
		if (io_buf_is_empty(&s->out)) {
			iod->events &= ~POLLOUT;
			if (iod->vmt->u.stream.all_sent)
				iod->vmt->u.stream.all_sent(iod);
			return 0;
		}
	}

	if (events & POLLIN) {
		if (s->connecting) {
			int err = 0;
			socklen_t len = sizeof (int);
			if (getsockopt(iod->fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
				errno = err;
				syslog(LOG_ERR, "connect to %s:%d failed (%m)", io_sock_hostoa(&s->sa), s->sa.port);
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

	io_stream_t *s = (io_stream_t *)d;
	io_buf_free(&s->out);
}


/* -------------------------------------------------------------------------- */
io_vmt_t io_stream_vmt = {
	.name = "io_stream",
	.ancestor = &io_d_vmt,
	.free = io_stream_free,
	.event = io_stream_event_handler
};


/* -------------------------------------------------------------------------- */
io_stream_t *io_stream_accept(io_stream_t *t, io_stream_listen_t *s, io_vmt_t *vmt)
{
	io_sock_addr_t sa;
	int sd;

	if (s->sa.family == AF_UNIX) {
		sd = accept4(s->d.fd, NULL, NULL, SOCK_NONBLOCK);
	} else {
		unisa_t usa;
		memset(&usa, 0, sizeof sa);
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
		t = (io_stream_t *)calloc(1, sizeof *t);

	io_d_init(&t->d, sd, POLLIN, vmt ?: &io_stream_vmt);
	io_buf_init(&t->out);

	if (s->sa.family == AF_UNIX)
		memset(&t->sa, 0, sizeof t->sa);
	else
		t->sa = sa;

	t->connecting = 0;

	return t;
}

/* -------------------------------------------------------------------------- */
io_stream_t *io_stream_connect(io_stream_t *t, io_sock_addr_t *sa, io_vmt_t *vmt)
{
	int fd = _io_stream_connect(sa);
	if (fd < 0)
		return NULL;

	if (!t)
		t = (io_stream_t *)calloc(1, sizeof *t);

	io_d_init(&t->d, fd, POLLIN, vmt ?: &io_stream_vmt);
	io_buf_init(&t->out);

	t->connecting = 1;
	t->sa = *sa;

	return t;
}

