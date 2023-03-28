#ifndef NANO_STREAM_H
#define NANO_STREAM_H

#include "nano/io_sock_addr.h"

typedef struct io_stream_listen io_stream_listen_t;

typedef struct io_stream io_stream_t;

/* -------------------------------------------------------------------------- */
typedef
struct stream_listen_conf {
	int queue_size;
	io_sock_addr_t sa;
	char iface[24];
} io_stream_listen_conf_t;


/* -------------------------------------------------------------------------- */
struct io_stream_listen {
	io_d_t d;
	io_sock_addr_t sa;
};

/* -------------------------------------------------------------------------- */

struct io_stream {
	io_d_t d;
	io_sock_addr_t sa;
	io_buf_t out;
	int connecting;
};


/* -------------------------------------------------------------------------- */
io_stream_listen_t *io_stream_listen_create(io_stream_listen_t *s, io_stream_listen_conf_t *conf, io_vmt_t *vmt);

io_stream_t *io_stream_accept (io_stream_t *bs, io_stream_listen_t *s, io_vmt_t *vmt);
io_stream_t *io_stream_connect(io_stream_t *bs, io_sock_addr_t *sa, io_vmt_t *vmt);


/* -------------------------------------------------------------------------- */
/* internals */
io_vmt_t io_stream_vmt;
io_vmt_t io_stream_listen_vmt;

void io_stream_free(io_d_t *d);
int io_stream_event_handler(io_d_t *iod, int events);

/* -------------------------------------------------------------------------- */
/* helpers */

#define io_stream_close(s)                 io_d_free(&(s)->d)
#define io_stream_recv(s, b, z)            io_d_recv(&(s)->d, b, z)

int io_stream_write(io_stream_t *s, void const *data, size_t size);
int io_stream_vwritef(io_stream_t *s, char const *fmt, va_list ap);
int io_stream_writef(io_stream_t *s, char const *fmt, ...) __attribute__ ((format (printf, 2, 3)));

#endif
