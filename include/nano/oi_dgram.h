#if 0 //ndef NANO_DGRAM_H
#define NANO_DGRAM_H

#include "nano/io_sock_addr.h"

typedef struct io_dgram_server io_dgram_server_t;
typedef struct io_dgram_serv_con io_dgram_serv_con_t;
typedef struct io_dgram_client io_dgram_client_t;

typedef
int (io_dgram_server_handler_t)(io_dgram_server_t *self, io_sock_addr_t *sa, uint8_t *data, size_t size);

typedef
int (io_dgram_serv_con_handler_t)(io_dgram_serv_con_t *self, uint8_t *data, size_t size);


/* -------------------------------------------------------------------------- */
typedef
struct dgram_server_conf {
	io_sock_addr_t sa;
	char iface[24];
} io_dgram_server_conf_t;


typedef struct io_dgram_serv_con io_dgram_serv_con_t;


/* -------------------------------------------------------------------------- */
struct io_dgram_server {
	io_d_t d;
	io_sock_addr_t sa;
	io_dgram_server_handler_t *recv_handler;

	io_dgram_serv_con_t *first;
};


/* -------------------------------------------------------------------------- */
struct io_dgram_serv_con {
	io_sock_addr_t sa;
	io_dgram_serv_con_handler_t *recv_handler;

	io_dgram_serv_con_t *next;
	io_dgram_server_t *server;
};


/* -------------------------------------------------------------------------- */
io_dgram_server_t *io_dgram_server_create(io_dgram_server_conf_t *conf, io_dgram_server_handler_t *handler);


/* -------------------------------------------------------------------------- */
/* internals */

void io_dgram_free(io_d_t *d);
int io_dgram_event_handler(io_d_t *d, int events);


/* -------------------------------------------------------------------------- */
/* helpers */

//int io_dgram_send(io_d_t *d, io_sock_addr_t const *sa, uint8_t const *buf, size_t size);

//int io_dgram_serv_con_send(io_dgram_serv_con_t *d, void const *buf, size_t size);

int io_dgram_client_send_static( io_dgram_client_t *d, void const *buf, size_t size);
int io_dgram_client_send_dynamic(io_dgram_client_t *d, void const *buf, size_t size);
int io_dgram_client_send_local(  io_dgram_client_t *d, void const *buf, size_t size);


#endif
