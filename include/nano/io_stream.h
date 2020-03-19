#ifndef NANO_STREAM_H
#define NANO_STREAM_H

#include "nano/io_sock_addr.h"

typedef struct io_stream_listen io_stream_listen_t;

typedef
int (io_stream_accept_handler_t)(io_stream_listen_t *self);


/* -------------------------------------------------------------------------- */
typedef
struct stream_listen_conf {
	int queue_size;
	io_sock_addr_t sock;
	char iface[24];
} io_stream_listen_conf_t;


/* -------------------------------------------------------------------------- */
struct io_stream_listen {
	io_d_t d;
	io_sock_addr_t conf;
	io_stream_accept_handler_t *accept_handler;
};

/* -------------------------------------------------------------------------- */
typedef
struct io_buf_sock {
	io_buf_d_t bd;
	io_sock_addr_t conf;
	int connecting;
	io_d_event_handler_t *pollin;
} io_buf_sock_t;


/* -------------------------------------------------------------------------- */
io_stream_listen_t *io_stream_listen_create(io_stream_listen_conf_t *conf, io_stream_accept_handler_t *handler);

io_buf_sock_t *io_stream_accept (io_buf_sock_t *t, io_stream_listen_t *s, io_d_event_handler_t *handler);
io_buf_sock_t *io_stream_connect(io_buf_sock_t *t, io_sock_addr_t *conf, io_d_event_handler_t *handler);


/* -------------------------------------------------------------------------- */
/* internals */
void io_stream_free(io_d_t *d);
int  io_stream_event_handler(io_d_t *d, int events);

/* -------------------------------------------------------------------------- */
/* helpers */

#define io_buf_sock_recv(bs, b, s, f)        recv((bs)->bd.d.fd, b, s, f)

#define io_buf_sock_vwritef(t, fmt, ap)      io_buf_d_vwritef(&(t)->bd.d, fmt, ap)
#define io_buf_sock_write(t, data, size)     io_buf_d_write(&(t)->bd.d, data, size)
#define io_buf_sock_writef(t, fmt...)        io_buf_d_writef(&(t)->bd.d, ## fmt)


#endif
