#ifndef NANO_STREAM_H
#define NANO_STREAM_H

#include "nano/io_buf_socks.h"

typedef
struct sock_conf {
	uint16_t port;
	uint16_t family;
	union {
		uint32_t ipv4;
		uint16_t ipv6[8];
		char const *unix;
	} addr;
} io_sock_conf;

typedef struct io_stream_listen io_stream_listen_t;

typedef
void (stream_accept_handler_t)(io_stream_listen_t *self, int sock, io_sock_conf_t *sock);


/* -------------------------------------------------------------------------- */
typedef
struct stream_listen_conf {
	int queue_size;
	io_sock_conf_t sock;
	char iface[24];
} io_stream_listen_conf_t;


/* -------------------------------------------------------------------------- */
struct io_stream_listen {
	io_sock_t sock;
	io_stream_conf_t ip;
	stream_accept_handler_t *accept_handler;
};


/* -------------------------------------------------------------------------- */
io_stream_listen_t *io_stream_listen_create(io_stream_listen_conf_t *conf, tcp_accept_handler_t *handler);


/* -------------------------------------------------------------------------- */
typedef
struct io_buf_stream {
	io_buf_sock_t sock;
	io_stream_conf_t conf;
	int connecting;
} io_buf_stream_t;


/* -------------------------------------------------------------------------- */
io_buf_stream_t *io_stream_accept (io_buf_stream_t *t, io_sock_t *listen_stream);
io_buf_stream_t *io_stream_connect(io_buf_stream_t *t, io_stream_conf_t *ip);

#endif
