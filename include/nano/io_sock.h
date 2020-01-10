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
} io_sock_addr_t;

typedef struct io_sock_listen io_sock_listen_t;

typedef
void (sock_accept_handler_t)(io_sock_listen_t *self, int sock, io_sock_addr_t *conf);


/* -------------------------------------------------------------------------- */
typedef
struct sock_listen_conf {
	int queue_size;
	io_sock_addr_t sock;
	char iface[24];
} io_sock_listen_conf_t;


/* -------------------------------------------------------------------------- */
struct io_sock_listen {
	io_d_t sock;
	io_sock_addr_t conf;
	sock_accept_handler_t *accept_handler;
};


/* -------------------------------------------------------------------------- */
io_sock_listen_t *io_sock_listen_create(io_sock_listen_conf_t *conf, sock_accept_handler_t *handler);
void io_sock_free(io_d_t *sock);


/* -------------------------------------------------------------------------- */
typedef
struct io_buf_sock {
	io_buf_d_t sock;
	io_sock_addr_t conf;
	int connecting;
} io_buf_sock_t;


/* -------------------------------------------------------------------------- */
io_buf_sock_t *io_sock_accept (io_buf_sock_t *t, io_d_t *listen_sock);
io_buf_sock_t *io_sock_connect(io_buf_sock_t *t, io_sock_addr_t *conf);

int io_sock_atohost(io_sock_addr_t *host, char const *a);

#endif
