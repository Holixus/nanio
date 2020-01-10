#ifndef NANO_STREAM_H
#define NANO_STREAM_H

typedef
struct sock_conf {
	uint16_t port;
	uint16_t family;
	union {
		char const *path;
		uint32_t ipv4;
		uint16_t ipv6[8];
	} addr;
} io_sock_conf_t;

typedef struct io_stream_listen io_stream_listen_t;

typedef
void (stream_accept_handler_t)(io_stream_listen_t *self, int sock, io_sock_conf_t *conf);


/* -------------------------------------------------------------------------- */
typedef
struct sock_listen_conf {
	int queue_size;
	io_sock_conf_t sock;
	char iface[24];
} io_sock_listen_conf_t;


/* -------------------------------------------------------------------------- */
struct io_stream_listen {
	io_sock_t sock;
	io_sock_conf_t conf;
	stream_accept_handler_t *accept_handler;
};


/* -------------------------------------------------------------------------- */
io_stream_listen_t *io_stream_listen_create(io_sock_listen_conf_t *conf, stream_accept_handler_t *handler);
void io_stream_free(io_sock_t *sock);


/* -------------------------------------------------------------------------- */
typedef
struct io_buf_stream {
	io_buf_sock_t sock;
	io_sock_conf_t conf;
	int connecting;
} io_buf_stream_t;


/* -------------------------------------------------------------------------- */
io_buf_stream_t *io_stream_accept (io_buf_stream_t *t, io_sock_t *listen_stream);
io_buf_stream_t *io_stream_connect(io_buf_stream_t *t, io_sock_conf_t *conf);

#endif
