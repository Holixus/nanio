#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#include <errno.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/ip.h>

#include "nano/io.h"
#include "nano/io_log.h"
#include "nano/io_ds.h"

/* -------------------------------------------------------------------------- */
static io_d_t *io_ds;
static size_t io_ds_length;

#ifndef NDEBUG
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
#endif

io_vmt_t io_d_vmt = {
	.name = "io_d"
};

/* -------------------------------------------------------------------------- */
static io_vmt_t *io_vmt_finish(io_vmt_t *vmt)
{
	if (!vmt->final && vmt->ancestor) {
		void **up = (void **)&vmt->ancestor->free;
		void **it = (void **)&vmt->free;
		void **t = it, **e = it + (sizeof *vmt - offsetof(io_vmt_t, free))/sizeof vmt;
		for (; t < e; ++t)
			if (!*t) // method skipped
				*t = up[t-it]; // copy method from ancestor vmt
		vmt->final = 1;
	}
	return vmt;
}

/* -------------------------------------------------------------------------- */
void io_d_init(io_d_t *self, int fd, int events, io_vmt_t *vmt)
{
	self->vmt = io_vmt_finish(vmt);
	self->fd = fd;
	self->events = events;
	self->next = io_ds;
	io_ds = self;
	io_ds_length += 1;
}

/* -------------------------------------------------------------------------- */
void io_d_close(io_d_t *self)
{
	if (self->fd >= 0) {
		close(self->fd);
		self->fd = -1;
	}
}

/* -------------------------------------------------------------------------- */
void io_d_free(io_d_t *self)
{
	for (io_d_t **s = &io_ds; s; s = &s[0]->next)
		if (*s == self) {
			*s = self->next;
			io_ds_length -= 1;
			break;
		}
	self->next = NULL;
	if (self->vmt->free)
		self->vmt->free(self);
	if (self->fd >= 0)
		close(self->fd);
	free(self);
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
ssize_t io_d_recv(io_d_t *self, void *buf, size_t len)
{
	ssize_t rcvd;
	do {
		rcvd = recv(self->fd, buf, len, 0);
	} while (rcvd < 0 && errno == EINTR);
	return rcvd;
}

/* -------------------------------------------------------------------------- */
int io_ds_poll(int timeout)
{
	size_t len = io_ds_length;
	struct pollfd fds[len];
	io_d_t *ds[len];

	size_t n = 0;
	for (io_d_t *d = io_ds; d; d = d->next) {
		if (d->vmt->idle)
			d->vmt->idle(d);
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

	for (size_t i = 0; i < n && ret; ++i)
		if (fds[i].revents/* & ds[i]->events*/) {
			//io_debug("%s event %s\n", ds[i]->vmt->name, pollevtoa(fds[i].revents));
			ds[i]->vmt->event(ds[i], fds[i].revents);
			if (ds[i]->fd == -1)
				io_d_free(ds[i]);
			--ret;
		}

	return (int)n;
}
