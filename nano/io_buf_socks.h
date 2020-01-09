#ifndef NANO_BUF_SOCKS_H
#define NANO_BUF_SOCKS_H

#include "nano/io_socks.h"

/* -------------------------------------------------------------------------- */
typedef
struct io_buf_sock {
	io_sock_t sock;
	io_buf_t out;
	io_sock_poll_handler_t *pollin;
} io_buf_sock_t;


/* -------------------------------------------------------------------------- */
io_buf_sock_t *io_buf_sock_create(io_buf_sock_t *t, int sock, io_sock_ops_t const *ops);

/* -------------------------------------------------------------------------- */
int io_buf_sock_write(io_sock_t *sock, char const *data, size_t size);
int io_buf_sock_vwritef(io_sock_t *sock, char const *fmt, va_list ap);
int io_buf_sock_writef(io_sock_t *sock, char const *fmt, ...) __attribute__ ((format (printf, 2, 3)));


/* -------------------------------------------------------------------------- */
/* internals */
void io_buf_sock_free(io_sock_t *sock);
void io_buf_sock_event_handler(io_sock_t *sock, int events);

#endif

