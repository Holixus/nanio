#include <stdio.h>
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

#include <sys/un.h>

#include <netinet/ip.h>

#include "nano/io.h"

#include "nano/io_ds.h"
#include "nano/io_buf.h"

#include "nano/io_dgram_client.h"

#include "nano/io_ipv4.h"
#include "nano/io_ipv6.h"


/* -------------------------------------------------------------------------- */
static int io_dgram_client_send(io_dgram_client_t *d, io_sock_addr_t const *to, void *buf, size_t size, int type)
{
	if (!size)
		return 0;

	if (io_queue_is_empty(&d->queue)) {
		unisa_t usa;
		ssize_t sent = sendto(d->d.fd, buf, size, 0, &usa.sa, io_sock_set_addr(&usa, to));
		if (sent >= 0)
			return sent;
		if (sent < 0 && errno != EAGAIN)
			return -1;
	}

	d->d.events |= POLLOUT;
	return io_queue_put_block(&d->queue, to, buf, size, type);
}

/* -------------------------------------------------------------------------- */
int io_dgram_client_send_static(io_dgram_client_t *d, io_sock_addr_t const *to, void const *buf, size_t size)
{
	return io_dgram_client_send(d, to, (void *)buf, size, IOQB_STATIC);
}

/* -------------------------------------------------------------------------- */
int io_dgram_client_send_dynamic(io_dgram_client_t *d, io_sock_addr_t const *to, void *buf, size_t size)
{
	return io_dgram_client_send(d, to, buf, size, IOQB_DYNAMIC);
}

/* -------------------------------------------------------------------------- */
int io_dgram_client_send_local(io_dgram_client_t *d, io_sock_addr_t const *to, void *buf, size_t size)
{
	return io_dgram_client_send(d, to, buf, size, IOQB_LOCAL);
}


/* -------------------------------------------------------------------------- */
static int io_dgram_recv(io_d_t *d, io_sock_addr_t *sa, void *buf, size_t size)
{
	unisa_t usa;
	socklen_t sa_size;
	int r = recvfrom(d->fd, buf, size, 0, &usa.sa, &sa_size);
	if (r >= 0)
		io_sock_get_addr(sa, &usa);
	return r;
}


/* -------------------------------------------------------------------------- */
static void io_dgram_client_free(io_d_t *d)
{
	io_dgram_client_t *self = (io_dgram_client_t *)d;
	io_queue_free(&self->queue);
}



/* -------------------------------------------------------------------------- */
static int io_dgram_client_event_handler(io_d_t *iod, int events)
{
	io_dgram_client_t *self = (io_dgram_client_t *)iod;
	if (events & POLLOUT) {
		io_queue_block_t *b = io_queue_get_block(&self->queue);
		if (b) {
			unisa_t usa;
			ssize_t sent = sendto(self->d.fd, b->data, b->size, 0, &usa.sa, io_sock_set_addr(&usa, (io_sock_addr_t const *)(b->tag)));
			if (sent >= 0) {
				io_queue_block_drop(&self->queue);
			} else {
				if (errno != EAGAIN)
					return -1;
			}
		}
		if (io_queue_is_empty(&self->queue))
			iod->events &= ~POLLOUT;
	}

	if (events & POLLIN && iod->vmt->u.dgram.recvd) {
		io_sock_addr_t addr;
		char buf[1500];

		ssize_t recvd = io_dgram_recv(iod, &addr, buf, sizeof buf);
		if (recvd > 0)
			iod->vmt->u.dgram.recvd(iod, &addr, buf, recvd);
		else
			if (recvd < 0)
				return -1;
	}

	return 0;
}


/* -------------------------------------------------------------------------- */
io_vmt_t io_dgram_client_vmt = {
	.name = "io_dgram_client",
	.ancestor = &io_d_vmt,
	.free = io_dgram_client_free,
	.event = io_dgram_client_event_handler
};


/* -------------------------------------------------------------------------- */
io_dgram_client_t *io_dgram_client_create(io_dgram_client_t *self, int family, char const *iface, io_vmt_t *vmt)
{
	int sd = io_socket(SOCK_DGRAM, family, iface);
	if (sd < 0)
		return NULL;

	if (!self)
		self = (io_dgram_client_t *)calloc(1, sizeof (io_dgram_client_t));

	io_d_init(&self->d, sd, POLLIN, vmt);

	io_queue_init(&self->queue);
	return self;
}

