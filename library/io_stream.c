#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
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
		io_err("fail to connect to %s:%d", io_sock_hostoa(sa), sa->port);
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

	if (d->fd == -1)
		return 0;

	uint8_t *dt = (uint8_t *)data;

	if (!s->connecting && io_buf_is_empty(&s->out)) {
		sent = send(d->fd, dt, size, 0);
		if (sent < 0) {
			io_stream_error(s, "send %u bytes failed", (unsigned)size);
			return -1;
		}
		dt += sent;
		size -= sent;
		if (sent && d->vmt->u.stream.sent)
			d->vmt->u.stream.sent(d, (unsigned)sent);
	}
	if (!size) {
		d->events &= ~POLLOUT;
		if (d->vmt->u.stream.all_sent)
			d->vmt->u.stream.all_sent(d);
		return 0;
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
		io_err("fail to listen socket to '%s'", io_sock_stoa(&conf->sa));
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
static int io_stream_connected(io_stream_t *s)
{
	//io_stream_debug(s, "connected");
	unisa_t usa;
	memset(&usa, 0, sizeof usa);
	socklen_t addrlen = sizeof usa;
	if (getpeername(s->d.fd, &usa.sa, &addrlen) < 0)
		io_err("fail to get peer address");
	else {
		if (usa.sa.sa_family != AF_UNIX)
			io_sock_get_addr(&s->sa, &usa);
		if (s->d.vmt->u.stream.connected)
			s->d.vmt->u.stream.connected(&s->d);
		if (io_buf_is_empty(&s->out))
			s->d.events &= ~POLLOUT;
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int io_stream_connecting(io_stream_t *s)
{
	//io_stream_debug(s, "connecting");
	int err = 0;
	socklen_t len = sizeof (int);
	if (getsockopt(s->d.fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
		// errno = err;
_fail:
		io_err("connect to %s:%d failed", io_sock_hostoa(&s->sa), s->sa.port);
		io_stream_close(s);
		return -1;
	}

	if (err)
		goto _fail;

	s->connecting = 0;
	return io_stream_connected(s);
}


/* -------------------------------------------------------------------------- */
int io_stream_event_handler(io_d_t *iod, int events)
{
	io_stream_t *s = (io_stream_t *)iod;

	if (s->connecting)
		return io_stream_connecting(s);

	if (events & POLLOUT) {
		int sent = io_buf_send(&s->out, iod->fd);
		if (sent < 0) {
			if (errno == ECONNRESET || errno == ENOTCONN || errno == EPIPE) {
				io_d_close(iod);
			} else
				return -1;
		}

		if (sent)
			if (iod->vmt->u.stream.sent)
				iod->vmt->u.stream.sent(iod, (unsigned)sent);

		if (io_buf_is_empty(&s->out)) {
			iod->events &= ~POLLOUT;
			if (iod->vmt->u.stream.all_sent)
				iod->vmt->u.stream.all_sent(iod);
			return 0;
		}
	}

	if (events & POLLIN) {
//		io_stream_debug(s, "receiving");
		if (iod->vmt->u.stream.recv) {
			if (iod->vmt->u.stream.recv(iod) < 0) {
				io_d_close(iod);
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
io_stream_t *io_stream_accept(io_stream_t *s, io_stream_listen_t *ls, io_vmt_t *vmt)
{
	unisa_t usa;

#if defined(BSD)
	socklen_t addrlen = sizeof usa;

	memset(&usa, 0, sizeof usa);
	int sd = accept(ls->d.fd, &usa.sa, &addrlen);
#else
	int sd;
	if (s->sa.family == AF_UNIX)
		sd = accept4(s->d.fd, NULL, NULL, SOCK_NONBLOCK);
	else {
		socklen_t addrlen = sizeof usa;

		memset(&usa, 0, sizeof usa);
		sd = accept4(s->d.fd, &usa.sa, &addrlen, SOCK_NONBLOCK);
	}
#endif
	if (sd < 0) {
		io_err("fail to accept connection");
		return NULL;
	}

	int in_progress = errno == EINPROGRESS;

	if (!s)
		s = (io_stream_t *)calloc(1, sizeof *s);

	io_d_init(&s->d, sd, POLLIN|(in_progress ? POLLOUT : 0), vmt ?: &io_stream_vmt);
	io_buf_init(&s->out);

	if (!in_progress && s->sa.family != AF_UNIX)
		io_sock_get_addr(&s->sa, &usa);

	s->connecting = in_progress;
	if (!s->connecting)
		io_stream_connected(s);
	return s;
}

/* -------------------------------------------------------------------------- */
io_stream_t *io_stream_connect(io_stream_t *s, io_sock_addr_t *sa, io_vmt_t *vmt)
{
	int fd = _io_stream_connect(sa);
	if (fd < 0)
		return NULL;

	int in_progress = errno == EINPROGRESS;

	if (!s)
		s = (io_stream_t *)calloc(1, sizeof *s);

	io_d_init(&s->d, fd, POLLIN|(in_progress ? POLLOUT : 0), vmt ?: &io_stream_vmt);
	io_buf_init(&s->out);

	s->sa = *sa;

	s->connecting = in_progress;
	if (!s->connecting)
		io_stream_connected(s);
	return s;
}

