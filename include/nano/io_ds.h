#ifndef NANO_SOCKS_H
#define NANO_SOCKS_H

#include "nano/io_sock_addr.h"


/* -------------------------------------------------------------------------- */
typedef struct io_d io_d_t;

typedef struct io_vmt io_vmt_t;

/* -------------------------------------------------------------------------- */
struct io_vmt {
	char const *name;
	io_vmt_t *ancestor;
	int final;
	void (*free    )(io_d_t *d);
	void (*idle    )(io_d_t *d);
	int  (*event   )(io_d_t *d, int events);
	union {
		struct {
			int  (*accept   )(io_d_t *d);
			int  (*connected)(io_d_t *d);
			int  (*recv     )(io_d_t *d);
			void (*sent     )(io_d_t *d, unsigned size);
			void (*all_sent )(io_d_t *d);
			int  (*close    )(io_d_t *d);
		} stream;
		struct {
			int  (*recvd   )(io_d_t *d, io_sock_addr_t const *from, void *data, size_t size); // datagram packet received
		} dgram;
	} u;
};

#define io_inherited   vmt->ancestor


/* -------------------------------------------------------------------------- */
struct io_d {
	io_vmt_t *vmt;
	io_d_t *next;
	int fd;
	int events;
};


/* -------------------------------------------------------------------------- */
void io_d_init(io_d_t *self, int fd, int events, io_vmt_t *vmt);
void io_d_close(io_d_t *self);
void io_d_free(io_d_t *self);

/* -------------------------------------------------------------------------- */
ssize_t io_d_recv(io_d_t *self, void *buf, size_t len);

extern io_vmt_t io_d_vmt;


/* -------------------------------------------------------------------------- */
/* internals */

void io_ds_init(void);
void io_ds_free(void);
int  io_ds_poll(int timeout);

#endif
