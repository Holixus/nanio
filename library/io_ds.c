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
#include "nano/io_ds.h"

/* -------------------------------------------------------------------------- */
static io_d_t *io_ds;
static size_t io_ds_length;

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
void io_d_init(io_d_t *self, int fd, int events, io_d_ops_t const *ops)
{
	self->fd = fd;
	self->events = events;
	self->ops = ops;
	self->next = io_ds;
	io_ds = self;
	io_ds_length += 1;
}

/* -------------------------------------------------------------------------- */
void io_d_free(io_d_t *self)
{
	for (io_d_t **s = &io_ds; s; s = &s[0]->next)
		if (*s == self) {
			*s = self->next;
			break;
		}
	if (self->ops->free)
		self->ops->free(self);
	close(self->fd);
	free(self);
	io_ds_length -= 1;
}

/* -------------------------------------------------------------------------- */
void io_ds_init()
{
	io_ds = NULL;
}

/* -------------------------------------------------------------------------- */
void io_ds_free()
{
	while (io_ds)
		io_d_free(io_ds);
}

/* -------------------------------------------------------------------------- */
int io_ds_poll(int timeout)
{
	size_t len = io_ds_length;
	struct pollfd fds[len];
	io_d_t *ds[len];

	size_t n = 0;
	for (io_d_t *d = io_ds; d; d = d->next) {
		if (d->ops->idle)
			d->ops->idle(d);
		if (d->events) {
			ds[n] = d;
			fds[n].fd = d->fd;
			fds[n].events = (short)d->events;
			++n;
		}
	}

	if (timeout < 0 && !n)
		return 0; // nothing to do

	int ret = poll(fds, n, timeout);
	if (ret < 0)
		return ret;

	for (size_t i = 0; i < n; ++i)
		if (fds[i].revents/* & ds[i]->events*/)
			ds[i]->ops->event(ds[i], fds[i].revents);

	return (int)n;
}
