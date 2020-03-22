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

#include <netinet/ip.h>

#include "nano/io.h"
#include "nano/io_ds.h"
#include "nano/io_buf.h"
#include "nano/io_buf_d.h"


/* -------------------------------------------------------------------------- */
void io_buf_d_free(io_d_t *d)
{
	io_buf_d_t *b = (io_buf_d_t *)d;
	io_buf_free(&b->out);
}

/* -------------------------------------------------------------------------- */
int io_buf_d_write(io_d_t *d, char const *data, size_t size)
{
	io_buf_d_t *b = (io_buf_d_t *)d;
	ssize_t sent = 0;

	if (io_buf_is_empty(&b->out)) {
		sent = send(d->fd, data, size, 0);
		if (sent < 0)
			return -1;
		data += sent;
		size -= sent;
	}
	d->events |= POLLOUT; // later call all_sent() method
	return size ? io_buf_write(&b->out, data, size) + sent : sent;
}

/* -------------------------------------------------------------------------- */
int io_buf_d_vwritef(io_d_t *d, char const *fmt, va_list ap)
{
	char msg[256];
	size_t len = (size_t)vsnprintf(msg, sizeof msg, fmt, ap);
	return io_buf_d_write(d, msg, len);
}

/* -------------------------------------------------------------------------- */
int io_buf_d_writef(io_d_t *d, char const *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = io_buf_d_vwritef(d, fmt, ap);
	va_end(ap);
	return r;
}

/* -------------------------------------------------------------------------- */
int io_buf_d_event_handler(io_d_t *d, int events)
{
	io_buf_d_t *b = (io_buf_d_t *)d;
	if (events & POLLOUT) {
		if (io_buf_send(&b->out, d->fd) < 0) {
			if (errno == ECONNRESET || errno == ENOTCONN || errno == EPIPE) {
				io_d_free(d);
			} else
				return -1;
		} else {
			if (io_buf_is_empty(&b->out)) {
				d->events &= ~POLLOUT;
				if (d->vmt->u.stream.all_sent)
					d->vmt->u.stream.all_sent(d);
			}
		}
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
io_vmt_t io_buf_d_vmt = {
	.name = "io_buf_d",
	.ancestor = &io_d_vmt,
	.free = io_buf_d_free,
	.event = io_buf_d_event_handler
};


/* -------------------------------------------------------------------------- */
io_buf_d_t *io_buf_d_create(io_buf_d_t *b, int fd, io_vmt_t *vmt)
{
	io_buf_init(&b->out);

	io_d_init(&b->d, fd, POLLIN, vmt ?: &io_buf_d_vmt);
	return b;
}

