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

#include "nano/io_socks.h"
#include "nano/io_buf.h"
#include "nano/io_buf_socks.h"


/* -------------------------------------------------------------------------- */
void io_buf_sock_free(io_sock_t *sock)
{
	io_buf_sock_t *p = (io_buf_sock_t *)sock;
	io_buf_free(&p->out);
}

/* -------------------------------------------------------------------------- */
int io_buf_sock_write(io_sock_t *sock, char const *data, size_t size)
{
	io_buf_sock_t *p = (io_buf_sock_t *)sock;

	if (io_buf_is_empty(&p->out)) {
		ssize_t sent = send(sock->fd, data, size, 0);
		if (sent < 0)
			return -1;
		if (sent == size)
			return size;
		data += sent;
		size -= sent;
	}
	return io_buf_write(&p->out, data, size);
}

/* -------------------------------------------------------------------------- */
int io_buf_sock_vwritef(io_sock_t *sock, char const *fmt, va_list ap)
{
	char msg[256];
	size_t len = (size_t)vsnprintf(msg, sizeof msg, fmt, ap);
	return io_buf_sock_write(sock, msg, len);
}

/* -------------------------------------------------------------------------- */
int io_buf_sock_writef(io_sock_t *sock, char const *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = io_buf_sock_vwritef(sock, fmt, ap);
	va_end(ap);
	return r;
}

/* -------------------------------------------------------------------------- */
void io_buf_sock_event_handler(io_sock_t *sock, int events)
{
	io_buf_sock_t *p = (io_buf_sock_t *)sock;
	if (events & POLLOUT) {
		if (io_buf_send(&p->out, sock->fd) < 0) {
			if (errno == ECONNRESET || errno == ENOTCONN || errno == EPIPE)
				io_sock_free(sock);
		}
	}
	if (events & POLLIN && p->pollin)
		p->pollin(sock);
}

/* -------------------------------------------------------------------------- */
static const io_sock_ops_t io_buf_sock_ops = {
	.free = io_buf_sock_free,
	.idle = NULL,
	.event = io_buf_sock_event_handler
};


/* -------------------------------------------------------------------------- */
io_buf_sock_t *io_buf_sock_create(io_buf_sock_t *t, int sock, io_sock_ops_t const *ops)
{
	io_buf_init(&t->out);

	io_sock_init(&t->sock, sock, POLLIN|POLLOUT, ops ?: &io_buf_sock_ops);
	return t;
}

