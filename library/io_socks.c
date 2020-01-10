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

#include "nano/io.h"
#include "nano/io_socks.h"

/* -------------------------------------------------------------------------- */
static io_sock_t *io_socks;
static size_t io_socks_length;

/* -------------------------------------------------------------------------- */
static char *pollevtoa(int ev)
{
	static char s[4][64];
	static int i = 0;
	char *b = s[++i & 3], *p = b;
	if (ev & POLLIN)
		p += sprintf(p, "|POLLIN");
	if (ev & POLLOUT)
		p += sprintf(p, "|POLLOUT");
	if (ev & POLLERR)
		p += sprintf(p, "|POLLERR");
	if (ev & POLLHUP)
		p += sprintf(p, "|POLLHUP");
	if ((ev &= ~(POLLIN|POLLOUT|POLLERR|POLLHUP)))
		p += sprintf(p, "|0x%02x", ev);
	return b + 1;
}

/* -------------------------------------------------------------------------- */
void io_sock_init(io_sock_t *self, int fd, int events, io_sock_ops_t const *ops)
{
	self->fd = fd;
	self->events = events;
	self->ops = ops;
	self->next = io_socks;
	io_socks = self;
	io_socks_length += 1;
}

/* -------------------------------------------------------------------------- */
void io_sock_free(io_sock_t *self)
{
	for (io_sock_t **s = &io_socks; s; s = &s[0]->next)
		if (*s == self) {
			*s = self->next;
			break;
		}
	if (self->ops->free)
		self->ops->free(self);
	close(self->fd);
	free(self);
	io_socks_length -= 1;
}

/* -------------------------------------------------------------------------- */
void io_socks_init()
{
	io_socks = NULL;
}

/* -------------------------------------------------------------------------- */
void io_socks_free()
{
	while (io_socks)
		io_sock_free(io_socks);
}

/* -------------------------------------------------------------------------- */
int io_socks_poll(int timeout)
{
	size_t len = io_socks_length;
	struct pollfd fds[len];
	io_sock_t *socks[len];

	size_t n = 0;
	for (io_sock_t *s = io_socks; s; s = s->next) {
		if (s->ops->idle)
			s->ops->idle(s);
		if (s->events) {
			socks[n] = s;
			fds[n].fd = s->fd;
			fds[n].events = (short)s->events;
			++n;
		}
	}

	if (timeout < 0 && !n)
		return 0; // nothing to do

	int ret = poll(fds, n, timeout);
	if (ret < 0)
		return ret;

	for (size_t i = 0; i < n; ++i)
		if (fds[i].revents/* & socks[i]->events*/)
			socks[i]->ops->event(socks[i], fds[i].revents);

	return (int)n;
}
