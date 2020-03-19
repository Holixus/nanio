#ifndef NANO_DGRAM_CLIENT_H
#define NANO_DGRAM_CLIENT_H

#include "nano/io_sock_addr.h"
#include "nano/io_queue.h"

typedef struct io_dgram_client io_dgram_client_t;

typedef
int (io_dgram_client_handler_t)(io_dgram_client_t *self, io_sock_addr_t const *from, void *data, size_t size);


/* -------------------------------------------------------------------------- */
struct io_dgram_client {
	io_d_t d;
	io_queue_t queue;
	io_dgram_client_handler_t *handler;
};


/* -------------------------------------------------------------------------- */
io_dgram_client_t *io_dgram_client_create(io_dgram_client_t *self, int family, char const *iface, io_dgram_client_handler_t *handler);


/* -------------------------------------------------------------------------- */
/* helpers */


int io_dgram_client_send_static( io_dgram_client_t *d, io_sock_addr_t const *to, void const *buf, size_t size);
int io_dgram_client_send_dynamic(io_dgram_client_t *d, io_sock_addr_t const *to, void *buf, size_t size);
int io_dgram_client_send_local(  io_dgram_client_t *d, io_sock_addr_t const *to, void *buf, size_t size);


#endif
