#ifndef NANO_STREAM_H
#define NANO_STREAM_H

#include "nano/io_sock_addr.h"

typedef struct io_stream_listen io_stream_listen_t;

typedef struct io_buf_sock io_buf_sock_t;

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
};

/* -------------------------------------------------------------------------- */

struct io_buf_sock {
	io_buf_d_t bd;
	io_sock_addr_t conf;
	int connecting;
};


/* -------------------------------------------------------------------------- */
io_stream_listen_t *io_stream_listen_create(io_stream_listen_t *s, io_stream_listen_conf_t *conf, io_vmt_t *vmt);

io_buf_sock_t *io_stream_accept (io_buf_sock_t *bs, io_stream_listen_t *s, io_vmt_t *vmt);
io_buf_sock_t *io_stream_connect(io_buf_sock_t *bs, io_sock_addr_t *conf, io_vmt_t *vmt);


/* -------------------------------------------------------------------------- */
/* internals */
io_vmt_t io_stream_vmt;
io_vmt_t io_stream_listen_vmt;

void io_stream_free(io_d_t *d);
int io_stream_event_handler(io_d_t *iod, int events);

/* -------------------------------------------------------------------------- */
/* helpers */

#define io_buf_sock_free(bs)                 io_d_free(&(bs)->bd.d)
#define io_buf_sock_recv(bs, b, s)           io_d_recv(&(bs)->bd.d, b, s)

#define io_buf_sock_vwritef(t, fmt, ap)      io_buf_d_vwritef(&(t)->bd.d, fmt, ap)
#define io_buf_sock_write(t, data, size)     io_buf_d_write(&(t)->bd.d, data, size)
#define io_buf_sock_writef(t, fmt...)        io_buf_d_writef(&(t)->bd.d, ## fmt)


#endif
