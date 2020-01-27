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
void (io_sock_accept_handler_t)(io_sock_listen_t *self);

typedef
void (io_event_handler_t)(io_d_t *iod, int events);


/* -------------------------------------------------------------------------- */
typedef
struct sock_listen_conf {
	int queue_size;
	io_sock_addr_t sock;
	char iface[24];
} io_sock_listen_conf_t;


/* -------------------------------------------------------------------------- */
struct io_sock_listen {
	io_d_t d;
	io_sock_addr_t conf;
	io_sock_accept_handler_t *accept_handler;
};

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
io_sock_listen_t *io_sock_listen_create(io_sock_listen_conf_t *conf, io_sock_accept_handler_t *handler);
void io_sock_free(io_d_t *sock);


/* -------------------------------------------------------------------------- */
typedef
struct io_buf_sock {
	io_buf_d_t bd;
	io_sock_addr_t conf;
	int connecting;
	io_event_handler_t *pollin;
} io_buf_sock_t;


/* -------------------------------------------------------------------------- */
io_buf_sock_t *io_sock_accept (io_buf_sock_t *t, io_sock_listen_t *s, io_event_handler_t *handler);
io_buf_sock_t *io_sock_connect(io_buf_sock_t *t, io_sock_addr_t *conf, io_event_handler_t *handler);


int io_sock_atohost(io_sock_addr_t *host, char const *a);
char const *io_sock_hostoa(io_sock_addr_t const *sc);

#define io_sock_free(t)                      io_buf_d_free(t)

#define io_buf_sock_recv(bs, b, s, f)        recv((bs)->bd.d.fd, b, s, f)

#define io_buf_sock_vwritef(t, fmt, ap)      io_buf_d_vwritef(&(t)->bd.d, fmt, ap)
#define io_buf_sock_write(t, data, size)     io_buf_d_write(&(t)->bd.d, data, size)
#define io_buf_sock_writef(t, fmt...)        io_buf_d_writef(&(t)->bd.d, ## fmt)


#endif
